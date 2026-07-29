// SPDX-License-Identifier: MIT

#include "polling_period_replay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace crv {
namespace {

constexpr auto lags = std::array<std::size_t, 4>{32, 64, 128, 256};
constexpr auto horizons = std::array<std::size_t, 5>{512, 1024, 2048, 4096, 8192};

struct percentile_t
{
    std::string_view name;
    std::uint32_t numerator;
    std::uint32_t denominator;
};

constexpr auto percentiles = std::array{
    percentile_t{"p0.1", 1, 1000},
    percentile_t{"p1", 1, 100},
    percentile_t{"p5", 1, 20},
    percentile_t{"p10", 1, 10},
};

auto sanitize_tsv(std::string_view value) -> std::string
{
    auto result = std::string{value};
    for (auto& character : result)
    {
        if (character == '\t' || character == '\r' || character == '\n') character = ' ';
    }
    return result;
}

auto median(std::vector<long double> values) -> long double
{
    if (values.empty()) throw std::logic_error{"median of empty population"};

    auto const middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    auto const upper = values[middle];

    if ((values.size() & 1U) != 0U) return upper;

    auto const lower = *std::max_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle));
    return (lower + upper) / 2.0L;
}

auto relative_distance(long double lhs, long double rhs) -> long double
{
    if (!(rhs > 0.0L)) return std::numeric_limits<long double>::infinity();
    return std::abs(lhs - rhs) / rhs;
}

auto seconds_from_ns(long double nanoseconds) -> long double
{
    return nanoseconds / 1'000'000'000.0L;
}

auto microseconds_from_ns(long double nanoseconds) -> long double
{
    return nanoseconds / 1'000.0L;
}

auto visible_lattice_saturation(std::size_t report_count, long double period_ns, std::uint64_t wall_duration_ns)
    -> long double
{
    if (report_count < 2 || wall_duration_ns == 0) return std::numeric_limits<long double>::infinity();

    return static_cast<long double>(report_count - 1U) * period_ns / static_cast<long double>(wall_duration_ns);
}

auto format_rank_mask(std::uint32_t mask) -> std::string
{
    auto stream = std::ostringstream{};
    auto first = true;
    for (auto index = std::size_t{}; index < percentiles.size(); ++index)
    {
        if ((mask & (std::uint32_t{1} << index)) == 0) continue;
        if (!first) stream << ',';
        stream << percentiles[index].name;
        first = false;
    }
    return first ? std::string{"none"} : stream.str();
}

auto format_horizon_mask(std::uint32_t mask) -> std::string
{
    auto stream = std::ostringstream{};
    auto first = true;
    for (auto index = std::size_t{}; index < horizons.size(); ++index)
    {
        if ((mask & (std::uint32_t{1} << index)) == 0) continue;
        if (!first) stream << ',';
        stream << horizons[index];
        first = false;
    }
    return first ? std::string{"none"} : stream.str();
}

} // namespace

class polling_period_replay_t::impl_t final
{
public:
    explicit impl_t(config_t config) : config_{std::move(config)}
    {
        validate_config();

        windows_.reserve(horizons.size());
        for (auto const horizon : horizons)
        {
            auto& window = windows_.emplace_back();
            window.horizon = horizon;
            window.timestamps.reserve(horizon);
        }

        if (config_.sidecar_path)
        {
            sidecar_.emplace(*config_.sidecar_path, std::ios::out | std::ios::trunc);
            if (!*sidecar_)
                throw std::runtime_error{"failed to open polling-period sidecar: " + config_.sidecar_path->string()};

            *sidecar_ << std::setprecision(18);
            write_sidecar_header();
        }
    }

    auto observe(std::uint64_t timestamp_ns) -> void
    {
        require_active("observe");

        if (last_timestamp_ns_ && timestamp_ns < *last_timestamp_ns_)
            throw std::runtime_error{
                "non-monotonic timestamp observed; reset() or break_observation_chain() is required"};

        if (!chain_first_timestamp_ns_) chain_first_timestamp_ns_ = timestamp_ns;

        auto const chain_report = chain_report_count_;
        auto const global_report = total_report_count_;

        for (auto& window : windows_)
        {
            if (window.timestamps.empty())
            {
                window.begin_chain_report = chain_report;
                window.begin_global_report = global_report;
            }
            window.timestamps.push_back(timestamp_ns);
        }

        last_timestamp_ns_ = timestamp_ns;
        ++chain_report_count_;
        ++total_report_count_;

        for (auto& window : windows_)
        {
            if (window.timestamps.size() == window.horizon)
            {
                process_complete_window(window, total_report_count_);
                window.timestamps.clear();
            }
        }
    }

    auto break_observation_chain(std::string_view reason) -> void
    {
        require_active("break_observation_chain");

        record_incomplete_windows("observation_chain_break: " + std::string{reason});
        close_current_chain_duration();
        ++observation_chain_breaks_;

        append_event_at(event_kind_t::observation_chain_broken, completed_duration_ns_, total_report_count_,
            std::nullopt, std::nullopt, sanitize_tsv(reason));

        begin_new_chain();
    }

    auto reset(std::string_view reason) -> void
    {
        require_active("reset");

        record_incomplete_windows("timestamp_chain_reset: " + std::string{reason});
        finalize_isolated_lower_clusters();

        if (qualified_cluster_index_)
        {
            ++invalidations_;
            append_event(event_kind_t::qualified_estimate_invalidated, clusters_[*qualified_cluster_index_].id,
                qualified_period_ns_, sanitize_tsv(reason));
            awaiting_reacquisition_ = true;
        }

        close_current_chain_duration();

        qualified_cluster_index_.reset();
        qualified_period_ns_.reset();
        cluster_center_at_qualification_ns_.reset();
        qualification_report_.reset();
        qualification_elapsed_ns_.reset();
        qualification_independent_evidence_ = 0;
        qualification_saturated_witnesses_ = 0;
        qualification_maximum_saturation_.reset();
        qualification_cluster_rank_mask_ = 0;
        qualification_cluster_horizon_mask_ = 0;
        qualification_independent_rank_mask_ = 0;
        qualification_independent_horizon_mask_ = 0;
        last_reinforcement_elapsed_ns_.reset();

        active_cluster_begin_ = clusters_.size();
        begin_new_chain();
    }

    auto finish() -> void
    {
        require_active("finish");

        record_incomplete_windows("end_of_capture");
        finalize_isolated_lower_clusters();
        finalize_holdover();
        close_current_chain_duration();

        if (sidecar_)
        {
            sidecar_->flush();
            if (!*sidecar_) throw std::runtime_error{"failed while writing polling-period sidecar"};
        }

        finished_ = true;
    }

    auto print(std::ostream& stream) const -> void
    {
        require_finished("print");

        auto const result = make_summary();

        stream << std::setprecision(6) << std::fixed;
        stream << "polling-period replay\n";
        stream << "  capture: " << config_.capture_identity << '\n';
        stream << "  reports: " << result.total_reports << '\n';
        stream << "  duration: " << result.duration_seconds << " s\n";
        stream << "  identification policy: " << config_.qualification_independent_evidence << " independent bundles, "
               << config_.qualification_saturated_witnesses << " saturated witness(es), saturation ["
               << config_.minimum_saturation << ", " << config_.maximum_sustainable_saturation << "]\n";

        if (result.first_candidate_report)
        {
            stream << "  first credible candidate: report " << *result.first_candidate_report << ", "
                   << *result.first_candidate_seconds << " s\n";
        }
        else
        {
            stream << "  first credible candidate: none\n";
        }

        if (result.qualification_report)
        {
            stream << "  qualification: report " << *result.qualification_report << ", "
                   << *result.qualification_seconds << " s\n";
            stream << "  operational frozen period: " << microseconds_from_ns(*result.qualified_period_ns) << " us\n";
            stream << "  cluster center at qualification: "
                   << microseconds_from_ns(*result.cluster_center_at_qualification_ns) << " us\n";
            stream << "  final diagnostic cluster center: "
                   << microseconds_from_ns(*result.final_qualified_cluster_center_ns) << " us\n";
            stream << "  qualification independent evidence bundles: " << result.qualification_independent_evidence
                   << '\n';
            stream << "  qualification saturated witnesses: " << result.qualification_saturated_witnesses << '\n';
            stream << "  qualification maximum independent saturation: " << *result.qualification_maximum_saturation
                   << '\n';
            stream << "  cluster candidate-rank support at qualification: "
                   << format_rank_mask(result.qualification_cluster_rank_mask) << '\n';
            stream << "  cluster horizon support at qualification: "
                   << format_horizon_mask(result.qualification_cluster_horizon_mask) << '\n';
            stream << "  independent-bundle rank support at qualification: "
                   << format_rank_mask(result.qualification_independent_rank_mask) << '\n';
            stream << "  independent-bundle horizon support at qualification: "
                   << format_horizon_mask(result.qualification_independent_horizon_mask) << '\n';
        }
        else
        {
            stream << "  qualification: none\n";
            stream << "  operational frozen period: none\n";
        }

        stream << "  credible candidate rows: " << result.credible_candidate_rows << '\n';
        stream << "  credible evidence intervals: " << result.credible_evidence_intervals << '\n';
        stream << "  credible evidence bundles: " << result.credible_evidence_bundles << '\n';
        stream << "  recurring clusters: " << result.recurring_clusters << '\n';
        stream << "  identified clusters: " << result.identified_clusters << '\n';
        stream << "  recurring but unidentified clusters: " << result.recurring_unidentified_clusters << '\n';
        stream << "  density-contradicted clusters: " << result.density_contradicted_clusters << '\n';
        stream << "  independent qualified-cluster bundles after qualification: "
               << result.independent_qualified_cluster_bundles << '\n';
        stream << "  rejected isolated lower clusters: " << result.rejected_isolated_lower_clusters << '\n';
        stream << "  rejected high candidate rows: " << result.rejected_high_candidate_rows << '\n';
        stream << "  rejected high evidence bundles: " << result.rejected_high_bundles << '\n';
        stream << "  poor-fit candidate rows: " << result.poor_fit_candidate_rows << '\n';
        stream << "  structural-failure candidate rows: " << result.structural_failure_candidate_rows << '\n';
        stream << "  incomplete-window candidate rows: " << result.incomplete_window_candidate_rows << '\n';
        stream << "  observation-chain breaks: " << result.observation_chain_breaks << '\n';
        stream << "  longest holdover: " << result.longest_holdover_seconds << " s\n";

        if (result.qualified_cluster_median_ns)
        {
            stream << "  qualified-cluster bundle range: " << microseconds_from_ns(*result.qualified_cluster_minimum_ns)
                   << " / " << microseconds_from_ns(*result.qualified_cluster_median_ns) << " / "
                   << microseconds_from_ns(*result.qualified_cluster_maximum_ns) << " us (min / median / max)\n";
            stream << "  qualified-cluster maximum independent saturation: "
                   << *result.qualified_cluster_maximum_independent_saturation << '\n';
            stream << "  qualified-cluster saturated witnesses: " << result.qualified_cluster_saturated_witnesses
                   << '\n';
            stream << "  qualified-cluster density contradicted: "
                   << (result.qualified_cluster_density_contradicted ? "yes" : "no") << '\n';
        }
        else
        {
            stream << "  qualified-cluster bundle range: none\n";
        }

        stream << "  invalidations: " << result.invalidations << '\n';
        stream << "  cluster switches: " << result.cluster_switches << '\n';
        stream << "  final state: " << (result.qualified ? "qualified" : "unqualified") << '\n';

        stream << "\nmeaningful transitions\n";
        if (events_.empty())
        {
            stream << "  none\n";
            return;
        }

        auto events = events_;
        std::stable_sort(events.begin(), events.end(), [](auto const& lhs, auto const& rhs) {
            if (lhs.global_report != rhs.global_report) return lhs.global_report < rhs.global_report;
            if (lhs.elapsed_ns != rhs.elapsed_ns) return lhs.elapsed_ns < rhs.elapsed_ns;
            return static_cast<int>(lhs.kind) < static_cast<int>(rhs.kind);
        });

        for (auto const& event : events)
        {
            stream << "  " << seconds_from_ns(event.elapsed_ns) << " s"
                   << " report " << event.global_report;
            if (event.cluster_id) stream << " cluster " << *event.cluster_id;
            if (event.period_ns) stream << " period " << microseconds_from_ns(*event.period_ns) << " us";
            stream << ": " << event_name(event.kind);
            if (!event.detail.empty()) stream << " (" << event.detail << ')';
            stream << '\n';
        }
    }

    auto make_summary() const -> summary_t
    {
        require_finished("summary");

        auto result = summary_t{};
        result.total_reports = total_report_count_;
        result.duration_seconds = seconds_from_ns(completed_duration_ns_);
        result.qualified = qualified_cluster_index_.has_value();
        result.qualified_period_ns = qualified_period_ns_;
        result.cluster_center_at_qualification_ns = cluster_center_at_qualification_ns_;
        result.first_candidate_report = first_candidate_report_;
        if (first_candidate_elapsed_ns_) result.first_candidate_seconds = seconds_from_ns(*first_candidate_elapsed_ns_);
        result.qualification_report = qualification_report_;
        if (qualification_elapsed_ns_) result.qualification_seconds = seconds_from_ns(*qualification_elapsed_ns_);
        result.qualification_independent_evidence = qualification_independent_evidence_;
        result.qualification_saturated_witnesses = qualification_saturated_witnesses_;
        result.qualification_maximum_saturation = qualification_maximum_saturation_;
        result.qualification_cluster_rank_mask = qualification_cluster_rank_mask_;
        result.qualification_cluster_horizon_mask = qualification_cluster_horizon_mask_;
        result.qualification_independent_rank_mask = qualification_independent_rank_mask_;
        result.qualification_independent_horizon_mask = qualification_independent_horizon_mask_;
        result.credible_candidate_rows = credible_candidate_rows_;
        result.credible_evidence_intervals = credible_evidence_intervals_;
        result.credible_evidence_bundles = credible_evidence_bundles_;
        for (auto index = active_cluster_begin_; index < clusters_.size(); ++index)
        {
            auto const& cluster = clusters_[index];
            if (cluster.recurring) ++result.recurring_clusters;
            if (cluster.identified) ++result.identified_clusters;
            if (cluster.recurring && !cluster.identified) ++result.recurring_unidentified_clusters;
            if (cluster.density_contradicted) ++result.density_contradicted_clusters;
        }
        result.independent_qualified_cluster_bundles = independent_qualified_cluster_bundles_;
        result.rejected_isolated_lower_clusters = rejected_isolated_lower_clusters_;
        result.rejected_high_candidate_rows = rejected_high_candidate_rows_;
        result.rejected_high_bundles = rejected_high_bundles_;
        result.poor_fit_candidate_rows = poor_fit_candidate_rows_;
        result.structural_failure_candidate_rows = structural_failure_candidate_rows_;
        result.incomplete_window_candidate_rows = incomplete_window_candidate_rows_;
        result.observation_chain_breaks = observation_chain_breaks_;
        result.longest_holdover_seconds = seconds_from_ns(longest_holdover_ns_);
        result.invalidations = invalidations_;
        result.cluster_switches = cluster_switches_;
        result.accepted_updates = independent_qualified_cluster_bundles_;
        result.rejected_high_candidates = rejected_high_candidate_rows_;

        if (last_qualified_cluster_index_)
        {
            auto const& cluster = clusters_[*last_qualified_cluster_index_];
            if (!cluster.independent_periods_ns.empty())
            {
                result.final_qualified_cluster_center_ns = cluster.center_ns;
                result.qualified_cluster_minimum_ns
                    = *std::min_element(cluster.independent_periods_ns.begin(), cluster.independent_periods_ns.end());
                result.qualified_cluster_median_ns = median(cluster.independent_periods_ns);
                result.qualified_cluster_maximum_ns
                    = *std::max_element(cluster.independent_periods_ns.begin(), cluster.independent_periods_ns.end());
                result.qualified_cluster_maximum_independent_saturation = cluster.maximum_independent_saturation;
                result.qualified_cluster_saturated_witnesses = cluster.saturated_witness_count;
                result.qualified_cluster_density_contradicted = cluster.density_contradicted;
            }
        }

        return result;
    }

private:
    struct window_t
    {
        std::size_t horizon{};
        std::vector<std::uint64_t> timestamps{};
        std::uint64_t begin_chain_report{};
        std::uint64_t begin_global_report{};
    };

    struct fit_t
    {
        long double intercept_ns{};
        long double slope_ns_reports{};
        long double mean_squared_error_ns2{};
        long double maximum_absolute_error_ns{};
    };

    enum class structural_status_t
    {
        accepted,
        incomplete_window,
        insufficient_support,
        invalid_fit,
    };

    enum class bundle_action_t
    {
        none,
        created,
        reinforced,
        corroborated,
    };

    enum class operational_decision_t
    {
        none,
        prequalification_evidence,
        recurring_unidentified,
        identification,
        qualified_cluster_update,
        qualified_cluster_corroboration,
        identified_challenger,
        rejected_high_holdover,
        lower_challenge,
        rejected_poor_fit,
        structural_failure,
    };

    struct candidate_t
    {
        std::uint64_t id{};
        std::uint64_t chain_id{};
        std::uint64_t begin_chain_report{};
        std::uint64_t end_chain_report{};
        std::uint64_t begin_global_report{};
        std::uint64_t end_global_report{};
        std::uint64_t begin_timestamp_ns{};
        std::uint64_t end_timestamp_ns{};
        std::uint64_t wall_duration_ns{};
        std::size_t horizon{};
        std::size_t horizon_index{};
        std::size_t percentile_index{};
        percentile_t percentile{};
        std::array<std::size_t, lags.size()> support{};
        std::array<std::optional<std::uint64_t>, lags.size()> selected_span_ns{};
        structural_status_t structural_status{structural_status_t::accepted};
        std::string failure_reason{};
        std::optional<fit_t> fit{};
        bool credible{};
        std::optional<long double> candidate_saturation{};

        std::optional<std::uint64_t> bundle_id{};
        std::optional<std::size_t> bundle_member_count{};
        std::optional<std::uint32_t> bundle_rank_mask{};
        std::optional<std::uint32_t> bundle_horizon_mask{};
        std::optional<long double> bundle_representative_intercept_ns{};
        std::optional<long double> bundle_saturation{};
        bool bundle_saturated_witness{};
        std::optional<std::uint64_t> bundle_target_cluster_id{};
        bool bundle_independent_evidence{};
        bundle_action_t bundle_action{bundle_action_t::none};
        operational_decision_t operational_decision{operational_decision_t::none};

        std::optional<long double> cluster_relative_distance{};
        std::optional<long double> cluster_center_ns{};
        std::optional<long double> cluster_median_absolute_deviation_ns{};
        std::optional<std::size_t> cluster_independent_evidence_count{};
        std::optional<std::uint32_t> cluster_supporting_rank_mask{};
        std::optional<std::uint32_t> cluster_supporting_horizon_mask{};
        std::optional<std::uint32_t> cluster_independent_rank_mask{};
        std::optional<std::uint32_t> cluster_independent_horizon_mask{};
        std::optional<long double> cluster_maximum_independent_saturation{};
        std::optional<std::size_t> cluster_saturated_witness_count{};
        std::optional<long double> cluster_maximum_observed_saturation{};
        std::optional<bool> cluster_density_contradicted{};
        std::optional<bool> cluster_recurring{};
        std::optional<bool> cluster_identified{};
        std::string cluster_identification_blocker{};
        long double availability_elapsed_ns{};
    };

    struct evidence_interval_t
    {
        std::uint64_t chain_id{};
        std::uint64_t begin_report{};
        std::uint64_t end_report{};
    };

    struct bundle_t
    {
        std::uint64_t id{};
        std::vector<std::size_t> candidate_indices{};
        std::optional<std::size_t> target_cluster_index{};
        std::size_t applied_cluster_index{};
        long double representative_intercept_ns{};
        long double saturation{};
        long double assignment_relative_distance{};
        std::uint32_t rank_mask{};
        std::uint32_t horizon_mask{};
        evidence_interval_t evidence{};
        std::uint64_t end_global_report{};
        std::uint64_t end_timestamp_ns{};
        long double availability_elapsed_ns{};
        bool independent_evidence{};
        bool saturated_witness{};
        bundle_action_t action{bundle_action_t::none};
        operational_decision_t operational_decision{operational_decision_t::none};
    };

    struct cluster_t
    {
        std::uint64_t id{};
        long double anchor_ns{};
        long double center_ns{};
        long double minimum_ns{};
        long double maximum_ns{};
        long double median_absolute_deviation_ns{};
        std::vector<long double> independent_periods_ns{};
        std::vector<long double> independent_saturations{};
        evidence_interval_t last_independent_interval{};
        std::uint64_t first_evidence_timestamp_ns{};
        std::uint64_t most_recent_evidence_timestamp_ns{};
        long double creation_elapsed_ns{};
        std::uint64_t creation_global_report{};
        long double last_independent_elapsed_ns{};
        std::size_t candidate_count{};
        std::size_t bundle_count{};
        std::uint32_t supporting_rank_mask{};
        std::uint32_t supporting_horizon_mask{};
        std::uint32_t independent_supporting_rank_mask{};
        std::uint32_t independent_supporting_horizon_mask{};
        long double maximum_independent_saturation{};
        long double maximum_observed_saturation{};
        std::size_t saturated_witness_count{};
        bool density_contradicted{};
        bool recurring{};
        bool identified{};
        bool recurrence_logged{};
        bool identification_logged{};
        bool contradiction_logged{};
        bool creation_logged{};
        bool qualified{};
        bool lower_than_qualified{};
        bool lower_recurring_logged{};
        bool high_rejection_logged{};
        bool isolated_lower_logged{};
    };

    enum class event_kind_t
    {
        cluster_created,
        cluster_reinforced,
        cluster_became_recurring,
        cluster_identified,
        cluster_density_contradicted,
        cluster_qualified,
        isolated_lower_outlier_rejected,
        higher_candidate_rejected_during_holdover,
        recurring_lower_cluster_detected,
        qualified_estimate_invalidated,
        reacquisition_completed,
        observation_chain_broken,
    };

    struct event_t
    {
        event_kind_t kind{};
        long double elapsed_ns{};
        std::uint64_t global_report{};
        std::optional<std::uint64_t> cluster_id{};
        std::optional<long double> period_ns{};
        std::string detail{};
    };

    auto validate_config() const -> void
    {
        if (config_.capture_identity.empty()) throw std::invalid_argument{"capture identity must not be empty"};
        if (!(config_.cluster_relative_tolerance > 0.0L))
            throw std::invalid_argument{"cluster relative tolerance must be positive"};
        if (!(config_.max_relative_fit_rmse > 0.0L))
            throw std::invalid_argument{"maximum relative fit RMSE must be positive"};
        if (config_.qualification_independent_evidence < 2)
            throw std::invalid_argument{"qualification requires at least two independent evidence intervals"};
        if (config_.qualification_saturated_witnesses == 0)
            throw std::invalid_argument{"qualification requires at least one saturated witness"};
        if (!(config_.minimum_saturation > 0.0L && config_.minimum_saturation <= 1.0L))
            throw std::invalid_argument{"minimum saturation must be in (0, 1]"};
        if (!(config_.maximum_sustainable_saturation >= 1.0L))
            throw std::invalid_argument{"maximum sustainable saturation must be at least 1"};
        if (!(config_.minimum_saturation <= config_.maximum_sustainable_saturation))
            throw std::invalid_argument{"minimum saturation must not exceed maximum sustainable saturation"};
    }

    auto require_active(std::string_view operation) const -> void
    {
        if (finished_) throw std::logic_error{std::string{operation} + " called after finish"};
    }

    auto require_finished(std::string_view operation) const -> void
    {
        if (!finished_) throw std::logic_error{std::string{operation} + " called before finish"};
    }

    auto current_elapsed_ns(std::uint64_t timestamp_ns) const -> long double
    {
        auto elapsed = completed_duration_ns_;
        if (chain_first_timestamp_ns_ && timestamp_ns >= *chain_first_timestamp_ns_)
            elapsed += static_cast<long double>(timestamp_ns - *chain_first_timestamp_ns_);
        return elapsed;
    }

    auto close_current_chain_duration() -> void
    {
        if (chain_first_timestamp_ns_ && last_timestamp_ns_ && *last_timestamp_ns_ >= *chain_first_timestamp_ns_)
            completed_duration_ns_ += static_cast<long double>(*last_timestamp_ns_ - *chain_first_timestamp_ns_);
    }

    auto begin_new_chain() -> void
    {
        ++chain_id_;
        chain_report_count_ = 0;
        chain_first_timestamp_ns_.reset();
        last_timestamp_ns_.reset();
        for (auto& window : windows_) window.timestamps.clear();
    }

    auto cluster_is_recurring(cluster_t const& cluster) const -> bool
    {
        return cluster.independent_periods_ns.size() >= config_.qualification_independent_evidence;
    }

    auto cluster_is_identified(cluster_t const& cluster) const -> bool
    {
        return cluster_is_recurring(cluster)
            && cluster.saturated_witness_count >= config_.qualification_saturated_witnesses
            && !cluster.density_contradicted;
    }

    auto cluster_identification_blocker(cluster_t const& cluster) const -> std::string
    {
        if (!cluster_is_recurring(cluster)) return "needs_independent_recurrence";
        if (cluster.saturated_witness_count < config_.qualification_saturated_witnesses)
            return "needs_saturated_witness";
        if (cluster.density_contradicted) return "density_contradicted";
        return "none";
    }

    auto mark_density_contradiction(cluster_t& cluster, long double saturation, long double availability_elapsed_ns,
        std::uint64_t availability_global_report) -> void
    {
        cluster.maximum_observed_saturation = std::max(cluster.maximum_observed_saturation, saturation);
        if (saturation <= config_.maximum_sustainable_saturation || cluster.density_contradicted) return;

        cluster.density_contradicted = true;
        cluster.identified = false;
        if (!cluster.contradiction_logged)
        {
            cluster.contradiction_logged = true;
            emit_cluster_creation(cluster);
            append_event_at(event_kind_t::cluster_density_contradicted, availability_elapsed_ns,
                availability_global_report, cluster.id, cluster.center_ns,
                "visible-lattice saturation " + std::to_string(static_cast<double>(saturation)));
        }
    }

    auto observe_window_density(window_t const& window, std::uint64_t availability_global_report) -> void
    {
        if (window.timestamps.size() < 2) return;

        auto const wall_duration_ns = window.timestamps.back() - window.timestamps.front();
        auto const availability_elapsed_ns = current_elapsed_ns(window.timestamps.back());
        for (auto index = active_cluster_begin_; index < clusters_.size(); ++index)
        {
            auto& cluster = clusters_[index];
            auto const saturation
                = visible_lattice_saturation(window.timestamps.size(), cluster.center_ns, wall_duration_ns);
            mark_density_contradiction(cluster, saturation, availability_elapsed_ns, availability_global_report);
        }
    }

    auto update_cluster_state_from_bundle(cluster_t& cluster, bundle_t const& bundle) -> void
    {
        mark_density_contradiction(
            cluster, bundle.saturation, bundle.availability_elapsed_ns, bundle.end_global_report);

        if (bundle.independent_evidence)
        {
            cluster.independent_saturations.push_back(bundle.saturation);
            cluster.maximum_independent_saturation
                = std::max(cluster.maximum_independent_saturation, bundle.saturation);
            if (bundle.saturated_witness) ++cluster.saturated_witness_count;
        }

        auto const recurring_now = cluster_is_recurring(cluster);
        if (recurring_now && !cluster.recurring)
        {
            cluster.recurring = true;
            if (!cluster.recurrence_logged)
            {
                cluster.recurrence_logged = true;
                emit_cluster_creation(cluster);
                append_event_at(event_kind_t::cluster_became_recurring, bundle.availability_elapsed_ns,
                    bundle.end_global_report, cluster.id, cluster.center_ns,
                    std::to_string(cluster.independent_periods_ns.size()) + " independent evidence bundles; "
                        + std::to_string(cluster.saturated_witness_count) + "/"
                        + std::to_string(config_.qualification_saturated_witnesses) + " saturated witnesses");
            }
        }

        auto const identified_now = cluster_is_identified(cluster);
        if (identified_now && !cluster.identified)
        {
            cluster.identified = true;
            if (!cluster.identification_logged)
            {
                cluster.identification_logged = true;
                emit_cluster_creation(cluster);
                append_event_at(event_kind_t::cluster_identified, bundle.availability_elapsed_ns,
                    bundle.end_global_report, cluster.id, cluster.center_ns,
                    std::to_string(cluster.saturated_witness_count) + " saturated witness(es), max saturation "
                        + std::to_string(static_cast<double>(cluster.maximum_independent_saturation)));
            }
        }
    }

    auto process_complete_window(window_t const& window, std::uint64_t availability_global_report) -> void
    {
        auto sorted_spans = std::array<std::vector<std::uint64_t>, lags.size()>{};

        for (auto lag_index = std::size_t{}; lag_index < lags.size(); ++lag_index)
        {
            auto const lag = lags[lag_index];
            auto& spans = sorted_spans[lag_index];
            spans.reserve(window.timestamps.size() - lag);

            for (auto index = lag; index < window.timestamps.size(); ++index)
            {
                auto const later = window.timestamps[index];
                auto const earlier = window.timestamps[index - lag];
                if (later < earlier)
                    throw std::runtime_error{"non-monotonic timestamp inside completed observation window"};
                spans.push_back(later - earlier);
            }

            std::sort(spans.begin(), spans.end());
        }

        observe_window_density(window, availability_global_report);

        auto candidates = std::vector<candidate_t>{};
        candidates.reserve(percentiles.size());

        for (auto percentile_index = std::size_t{}; percentile_index < percentiles.size(); ++percentile_index)
        {
            auto candidate = make_candidate_base(window, availability_global_report, percentile_index);
            auto sufficient = true;
            auto const percentile = percentiles[percentile_index];
            auto const minimum_support = (percentile.denominator + percentile.numerator - 1U) / percentile.numerator;

            for (auto lag_index = std::size_t{}; lag_index < lags.size(); ++lag_index)
            {
                auto const& spans = sorted_spans[lag_index];
                candidate.support[lag_index] = spans.size();

                if (spans.size() < minimum_support)
                {
                    sufficient = false;
                    continue;
                }

                auto const scaled_rank = static_cast<std::uint64_t>(percentile.numerator) * spans.size();
                auto const rank = (scaled_rank + percentile.denominator - 1U) / percentile.denominator;
                if (rank == 0 || rank > spans.size())
                    throw std::logic_error{"nearest-rank percentile calculation produced an invalid rank"};

                candidate.selected_span_ns[lag_index] = spans[rank - 1U];
            }

            if (!sufficient)
            {
                candidate.structural_status = structural_status_t::insufficient_support;
                candidate.failure_reason = "minimum percentile support not met at every lag";
                candidate.operational_decision = operational_decision_t::structural_failure;
                ++structural_failure_candidate_rows_;
                candidates.push_back(std::move(candidate));
                continue;
            }

            candidate.fit = fit_candidate(candidate.selected_span_ns);
            if (!candidate.fit || !(candidate.fit->intercept_ns > 0.0L) || !std::isfinite(candidate.fit->intercept_ns)
                || !std::isfinite(candidate.fit->slope_ns_reports)
                || !std::isfinite(candidate.fit->mean_squared_error_ns2)
                || !std::isfinite(candidate.fit->maximum_absolute_error_ns))
            {
                candidate.structural_status = structural_status_t::invalid_fit;
                candidate.failure_reason = "four-lag regression produced a non-finite or non-positive result";
                candidate.operational_decision = operational_decision_t::structural_failure;
                ++structural_failure_candidate_rows_;
                candidates.push_back(std::move(candidate));
                continue;
            }

            candidate.candidate_saturation = visible_lattice_saturation(
                candidate.horizon, candidate.fit->intercept_ns, candidate.wall_duration_ns);

            auto const allowed_rmse_ns = config_.max_relative_fit_rmse * candidate.fit->intercept_ns;
            candidate.credible = candidate.fit->mean_squared_error_ns2 <= allowed_rmse_ns * allowed_rmse_ns;

            if (!candidate.credible)
            {
                candidate.operational_decision = operational_decision_t::rejected_poor_fit;
                ++poor_fit_candidate_rows_;
                candidates.push_back(std::move(candidate));
                continue;
            }

            ++credible_candidate_rows_;
            candidates.push_back(std::move(candidate));
        }

        auto const credible = std::any_of(
            candidates.begin(), candidates.end(), [](auto const& candidate) { return candidate.credible; });

        if (credible)
        {
            if (!first_candidate_report_)
            {
                first_candidate_report_ = availability_global_report;
                first_candidate_elapsed_ns_ = candidates.front().availability_elapsed_ns;
            }
            apply_candidate_bundles(candidates);
        }

        for (auto const& candidate : candidates) write_candidate(candidate);
    }

    auto make_candidate_base(
        window_t const& window, std::uint64_t availability_global_report, std::size_t percentile_index) -> candidate_t
    {
        auto const horizon_iterator = std::find(horizons.begin(), horizons.end(), window.horizon);
        if (horizon_iterator == horizons.end()) throw std::logic_error{"unknown observation horizon"};

        auto candidate = candidate_t{};
        candidate.id = next_candidate_id_++;
        candidate.chain_id = chain_id_;
        candidate.begin_chain_report = window.begin_chain_report;
        candidate.end_chain_report = window.begin_chain_report + window.timestamps.size();
        candidate.begin_global_report = window.begin_global_report;
        candidate.end_global_report = window.begin_global_report + window.timestamps.size();
        candidate.begin_timestamp_ns = window.timestamps.front();
        candidate.end_timestamp_ns = window.timestamps.back();
        candidate.wall_duration_ns = candidate.end_timestamp_ns - candidate.begin_timestamp_ns;
        candidate.horizon = window.horizon;
        candidate.horizon_index = static_cast<std::size_t>(horizon_iterator - horizons.begin());
        candidate.percentile_index = percentile_index;
        candidate.percentile = percentiles[percentile_index];
        candidate.availability_elapsed_ns = current_elapsed_ns(candidate.end_timestamp_ns);

        if (candidate.end_global_report != availability_global_report)
            throw std::logic_error{"completed window report accounting mismatch"};

        return candidate;
    }

    auto fit_candidate(std::array<std::optional<std::uint64_t>, lags.size()> const& spans) const -> std::optional<fit_t>
    {
        auto sum_y = 0.0L;
        auto sum_uy = 0.0L;

        for (auto index = std::size_t{}; index < lags.size(); ++index)
        {
            if (!spans[index]) return std::nullopt;

            auto const u = static_cast<long double>(256U / lags[index]);
            auto const y = u * static_cast<long double>(*spans[index]);
            sum_y += y;
            sum_uy += u * y;
        }

        // For u = {8, 4, 2, 1}: sum(u) = 15, sum(u^2) = 85,
        // and the ordinary least-squares denominator is 115.
        auto const intercept_numerator = 85.0L * sum_y - 15.0L * sum_uy;
        auto const slope_numerator = 4.0L * sum_uy - 15.0L * sum_y;

        auto result = fit_t{};
        result.intercept_ns = intercept_numerator / (115.0L * 256.0L);
        result.slope_ns_reports = slope_numerator / 115.0L;

        auto squared_error_sum = 0.0L;
        auto maximum_absolute_error = 0.0L;
        for (auto index = std::size_t{}; index < lags.size(); ++index)
        {
            auto const lag = static_cast<long double>(lags[index]);
            auto const observed = static_cast<long double>(*spans[index]) / lag;
            auto const predicted = result.intercept_ns + result.slope_ns_reports / lag;
            auto const residual = observed - predicted;
            squared_error_sum += residual * residual;
            maximum_absolute_error = std::max(maximum_absolute_error, std::abs(residual));
        }

        result.mean_squared_error_ns2 = squared_error_sum / static_cast<long double>(lags.size());
        result.maximum_absolute_error_ns = maximum_absolute_error;
        return result;
    }

    auto apply_candidate_bundles(std::vector<candidate_t>& candidates) -> void
    {
        auto const cluster_snapshot_end = clusters_.size();
        auto bundles = build_bundles(candidates, cluster_snapshot_end);
        if (bundles.empty()) return;

        ++credible_evidence_intervals_;
        credible_evidence_bundles_ += bundles.size();

        auto const was_qualified = qualified_cluster_index_.has_value();
        for (auto& bundle : bundles) apply_bundle(bundle, candidates);

        if (!was_qualified) try_qualify(bundles.front().end_global_report, bundles.front().availability_elapsed_ns);

        for (auto& bundle : bundles)
        {
            classify_bundle(bundle, candidates, was_qualified);
            annotate_bundle_candidates(bundle, candidates);
        }
    }

    auto build_bundles(std::vector<candidate_t> const& candidates, std::size_t cluster_snapshot_end) const
        -> std::vector<bundle_t>
    {
        struct pending_group_t
        {
            std::optional<std::size_t> target_cluster_index{};
            std::vector<std::size_t> candidate_indices{};
            long double anchor_ns{};
            long double center_ns{};
        };

        auto groups = std::vector<pending_group_t>{};
        auto unmatched = std::vector<std::size_t>{};

        for (auto index = std::size_t{}; index < candidates.size(); ++index)
        {
            auto const& candidate = candidates[index];
            if (!candidate.credible) continue;

            auto const target = find_compatible_cluster(candidate.fit->intercept_ns, cluster_snapshot_end);
            if (!target)
            {
                unmatched.push_back(index);
                continue;
            }

            auto iterator = std::find_if(groups.begin(), groups.end(),
                [target](auto const& group) { return group.target_cluster_index == target; });
            if (iterator == groups.end())
            {
                auto& group = groups.emplace_back();
                group.target_cluster_index = target;
                group.candidate_indices.push_back(index);
                group.anchor_ns = candidate.fit->intercept_ns;
                group.center_ns = candidate.fit->intercept_ns;
            }
            else
            {
                iterator->candidate_indices.push_back(index);
            }
        }

        std::sort(unmatched.begin(), unmatched.end(), [&candidates](auto lhs, auto rhs) {
            auto const left = candidates[lhs].fit->intercept_ns;
            auto const right = candidates[rhs].fit->intercept_ns;
            return left < right
                || (left == right && candidates[lhs].percentile_index < candidates[rhs].percentile_index);
        });

        for (auto const candidate_index : unmatched)
        {
            auto const period = candidates[candidate_index].fit->intercept_ns;
            auto matched = false;

            for (auto& group : groups)
            {
                if (group.target_cluster_index) continue;
                if (relative_distance(period, group.center_ns) > config_.cluster_relative_tolerance) continue;
                if (relative_distance(period, group.anchor_ns) > 2.0L * config_.cluster_relative_tolerance) continue;

                group.candidate_indices.push_back(candidate_index);
                auto values = std::vector<long double>{};
                values.reserve(group.candidate_indices.size());
                for (auto const index : group.candidate_indices) values.push_back(candidates[index].fit->intercept_ns);
                group.center_ns = median(std::move(values));
                matched = true;
                break;
            }

            if (!matched)
            {
                auto& group = groups.emplace_back();
                group.candidate_indices.push_back(candidate_index);
                group.anchor_ns = period;
                group.center_ns = period;
            }
        }

        auto bundles = std::vector<bundle_t>{};
        bundles.reserve(groups.size());

        for (auto& group : groups)
        {
            auto bundle = bundle_t{};
            bundle.id = next_bundle_id_ + bundles.size();
            bundle.candidate_indices = std::move(group.candidate_indices);
            bundle.target_cluster_index = group.target_cluster_index;

            auto values = std::vector<long double>{};
            values.reserve(bundle.candidate_indices.size());
            for (auto const index : bundle.candidate_indices)
            {
                auto const& candidate = candidates[index];
                values.push_back(candidate.fit->intercept_ns);
                bundle.rank_mask |= std::uint32_t{1} << candidate.percentile_index;
                bundle.horizon_mask |= std::uint32_t{1} << candidate.horizon_index;
            }
            bundle.representative_intercept_ns = median(std::move(values));

            auto const& first = candidates[bundle.candidate_indices.front()];
            bundle.saturation
                = visible_lattice_saturation(first.horizon, bundle.representative_intercept_ns, first.wall_duration_ns);
            bundle.saturated_witness = bundle.saturation >= config_.minimum_saturation
                && bundle.saturation <= config_.maximum_sustainable_saturation;
            bundle.evidence = {
                first.chain_id,
                first.begin_chain_report,
                first.end_chain_report,
            };
            bundle.end_global_report = first.end_global_report;
            bundle.end_timestamp_ns = first.end_timestamp_ns;
            bundle.availability_elapsed_ns = first.availability_elapsed_ns;
            bundles.push_back(std::move(bundle));
        }

        std::sort(bundles.begin(), bundles.end(), [](auto const& lhs, auto const& rhs) {
            if (lhs.representative_intercept_ns != rhs.representative_intercept_ns)
                return lhs.representative_intercept_ns < rhs.representative_intercept_ns;
            return lhs.id < rhs.id;
        });

        for (auto index = std::size_t{}; index < bundles.size(); ++index) bundles[index].id = next_bundle_id_ + index;

        return bundles;
    }

    auto apply_bundle(bundle_t& bundle, std::vector<candidate_t> const& candidates) -> void
    {
        if (bundle.target_cluster_index) { reinforce_cluster(bundle, *bundle.target_cluster_index, candidates); }
        else
        {
            create_cluster(bundle, candidates);
        }
        ++next_bundle_id_;
    }

    auto find_compatible_cluster(long double period_ns, std::size_t cluster_end) const -> std::optional<std::size_t>
    {
        auto best = std::optional<std::size_t>{};
        auto best_distance = std::numeric_limits<long double>::infinity();

        for (auto index = active_cluster_begin_; index < cluster_end; ++index)
        {
            auto const& cluster = clusters_[index];
            auto const center_distance = relative_distance(period_ns, cluster.center_ns);
            auto const anchor_distance = relative_distance(period_ns, cluster.anchor_ns);

            if (center_distance > config_.cluster_relative_tolerance) continue;
            if (anchor_distance > 2.0L * config_.cluster_relative_tolerance) continue;

            if (center_distance < best_distance
                || (center_distance == best_distance && (!best || cluster.center_ns < clusters_[*best].center_ns)))
            {
                best = index;
                best_distance = center_distance;
            }
        }

        return best;
    }

    auto create_cluster(bundle_t& bundle, std::vector<candidate_t> const& candidates) -> void
    {
        auto const& first = candidates[bundle.candidate_indices.front()];
        auto cluster = cluster_t{};
        cluster.id = next_cluster_id_++;
        cluster.anchor_ns = bundle.representative_intercept_ns;
        cluster.center_ns = bundle.representative_intercept_ns;
        cluster.minimum_ns = bundle.representative_intercept_ns;
        cluster.maximum_ns = bundle.representative_intercept_ns;
        cluster.independent_periods_ns.push_back(bundle.representative_intercept_ns);
        cluster.last_independent_interval = bundle.evidence;
        cluster.first_evidence_timestamp_ns = first.begin_timestamp_ns;
        cluster.most_recent_evidence_timestamp_ns = bundle.end_timestamp_ns;
        cluster.creation_elapsed_ns = bundle.availability_elapsed_ns;
        cluster.creation_global_report = bundle.end_global_report;
        cluster.last_independent_elapsed_ns = bundle.availability_elapsed_ns;
        cluster.candidate_count = bundle.candidate_indices.size();
        cluster.bundle_count = 1;
        cluster.supporting_rank_mask = bundle.rank_mask;
        cluster.supporting_horizon_mask = bundle.horizon_mask;
        cluster.independent_supporting_rank_mask = bundle.rank_mask;
        cluster.independent_supporting_horizon_mask = bundle.horizon_mask;

        bundle.applied_cluster_index = clusters_.size();
        bundle.assignment_relative_distance = 0.0L;
        bundle.independent_evidence = true;
        bundle.action = bundle_action_t::created;

        clusters_.push_back(std::move(cluster));
        update_cluster_state_from_bundle(clusters_.back(), bundle);
    }

    auto reinforce_cluster(bundle_t& bundle, std::size_t cluster_index, std::vector<candidate_t> const& candidates)
        -> void
    {
        auto& cluster = clusters_[cluster_index];
        auto const& first = candidates[bundle.candidate_indices.front()];
        bundle.applied_cluster_index = cluster_index;
        bundle.assignment_relative_distance = relative_distance(bundle.representative_intercept_ns, cluster.center_ns);
        bundle.independent_evidence = bundle.evidence.chain_id != cluster.last_independent_interval.chain_id
            || bundle.evidence.begin_report >= cluster.last_independent_interval.end_report;
        bundle.action = bundle.independent_evidence ? bundle_action_t::reinforced : bundle_action_t::corroborated;

        cluster.candidate_count += bundle.candidate_indices.size();
        ++cluster.bundle_count;
        cluster.most_recent_evidence_timestamp_ns = bundle.end_timestamp_ns;
        cluster.supporting_rank_mask |= bundle.rank_mask;
        cluster.supporting_horizon_mask |= bundle.horizon_mask;

        if (bundle.independent_evidence)
        {
            cluster.independent_periods_ns.push_back(bundle.representative_intercept_ns);
            cluster.last_independent_interval = bundle.evidence;
            cluster.last_independent_elapsed_ns = bundle.availability_elapsed_ns;
            cluster.independent_supporting_rank_mask |= bundle.rank_mask;
            cluster.independent_supporting_horizon_mask |= bundle.horizon_mask;
            recompute_cluster_statistics(cluster);
        }

        update_cluster_state_from_bundle(cluster, bundle);

        if (!bundle.independent_evidence) return;

        emit_cluster_creation(cluster);
        if (cluster.independent_periods_ns.size() == 2)
        {
            append_event_at(event_kind_t::cluster_reinforced, bundle.availability_elapsed_ns, bundle.end_global_report,
                cluster.id, cluster.center_ns, "second independent evidence interval");
        }

        static_cast<void>(first);
    }

    auto recompute_cluster_statistics(cluster_t& cluster) -> void
    {
        cluster.center_ns = median(cluster.independent_periods_ns);
        cluster.minimum_ns
            = *std::min_element(cluster.independent_periods_ns.begin(), cluster.independent_periods_ns.end());
        cluster.maximum_ns
            = *std::max_element(cluster.independent_periods_ns.begin(), cluster.independent_periods_ns.end());

        auto deviations = std::vector<long double>{};
        deviations.reserve(cluster.independent_periods_ns.size());
        for (auto const period : cluster.independent_periods_ns)
            deviations.push_back(std::abs(period - cluster.center_ns));
        cluster.median_absolute_deviation_ns = median(std::move(deviations));
    }

    auto try_qualify(std::uint64_t availability_global_report, long double availability_elapsed_ns) -> void
    {
        auto selected = std::optional<std::size_t>{};

        for (auto index = active_cluster_begin_; index < clusters_.size(); ++index)
        {
            auto const& cluster = clusters_[index];
            if (!cluster.identified) continue;

            // Identification already excludes finer under-occupied harmonics and
            // coarser density-contradicted lattices. Prefer the coarsest identified
            // visible lattice among the remaining candidates.
            if (!selected || cluster.center_ns > clusters_[*selected].center_ns) selected = index;
        }

        if (!selected) return;

        qualified_cluster_index_ = selected;
        last_qualified_cluster_index_ = selected;
        auto& cluster = clusters_[*selected];
        cluster.qualified = true;
        qualified_period_ns_ = cluster.center_ns;
        cluster_center_at_qualification_ns_ = cluster.center_ns;
        qualification_report_ = availability_global_report;
        qualification_elapsed_ns_ = availability_elapsed_ns;
        qualification_independent_evidence_ = cluster.independent_periods_ns.size();
        qualification_saturated_witnesses_ = cluster.saturated_witness_count;
        qualification_maximum_saturation_ = cluster.maximum_independent_saturation;
        qualification_cluster_rank_mask_ = cluster.supporting_rank_mask;
        qualification_cluster_horizon_mask_ = cluster.supporting_horizon_mask;
        qualification_independent_rank_mask_ = cluster.independent_supporting_rank_mask;
        qualification_independent_horizon_mask_ = cluster.independent_supporting_horizon_mask;
        last_reinforcement_elapsed_ns_ = cluster.last_independent_elapsed_ns;

        for (auto index = active_cluster_begin_; index < clusters_.size(); ++index)
        {
            if (index != *selected && clusters_[index].center_ns < *qualified_period_ns_)
                clusters_[index].lower_than_qualified = true;
        }

        emit_cluster_creation(cluster);
        append_event_at(event_kind_t::cluster_qualified, availability_elapsed_ns, availability_global_report,
            cluster.id, *qualified_period_ns_,
            std::to_string(cluster.independent_periods_ns.size()) + " independent evidence bundles, "
                + std::to_string(cluster.saturated_witness_count) + " saturated witness(es)");

        if (awaiting_reacquisition_)
        {
            awaiting_reacquisition_ = false;
            append_event_at(event_kind_t::reacquisition_completed, availability_elapsed_ns, availability_global_report,
                cluster.id, *qualified_period_ns_, {});
        }
    }

    auto classify_bundle(bundle_t& bundle, std::vector<candidate_t> const& candidates, bool was_qualified) -> void
    {
        if (!qualified_cluster_index_)
        {
            auto const& assigned = clusters_[bundle.applied_cluster_index];
            bundle.operational_decision = assigned.recurring ? operational_decision_t::recurring_unidentified
                                                             : operational_decision_t::prequalification_evidence;
            return;
        }

        auto& assigned = clusters_[bundle.applied_cluster_index];

        if (!was_qualified && bundle.applied_cluster_index == *qualified_cluster_index_)
        {
            bundle.operational_decision = operational_decision_t::identification;
            return;
        }

        if (bundle.applied_cluster_index == *qualified_cluster_index_)
        {
            if (bundle.independent_evidence)
            {
                bundle.operational_decision = operational_decision_t::qualified_cluster_update;
                if (was_qualified)
                {
                    ++independent_qualified_cluster_bundles_;
                    update_holdover(bundle.availability_elapsed_ns);
                }
            }
            else
            {
                bundle.operational_decision = operational_decision_t::qualified_cluster_corroboration;
            }
            return;
        }

        if (assigned.identified)
        {
            bundle.operational_decision = operational_decision_t::identified_challenger;
            return;
        }

        if (assigned.center_ns > *qualified_period_ns_)
        {
            bundle.operational_decision = operational_decision_t::rejected_high_holdover;
            ++rejected_high_bundles_;
            rejected_high_candidate_rows_ += bundle.candidate_indices.size();

            if (!assigned.high_rejection_logged)
            {
                assigned.high_rejection_logged = true;
                emit_cluster_creation(assigned);
                append_event_at(event_kind_t::higher_candidate_rejected_during_holdover, bundle.availability_elapsed_ns,
                    bundle.end_global_report, assigned.id, assigned.center_ns, "qualified period retained");
            }
            return;
        }

        bundle.operational_decision = operational_decision_t::lower_challenge;
        assigned.lower_than_qualified = true;

        if (assigned.recurring && !assigned.lower_recurring_logged)
        {
            assigned.lower_recurring_logged = true;
            emit_cluster_creation(assigned);
            append_event_at(event_kind_t::recurring_lower_cluster_detected, bundle.availability_elapsed_ns,
                bundle.end_global_report, assigned.id, assigned.center_ns, "baseline policy flags but does not switch");
        }

        static_cast<void>(candidates);
    }

    auto annotate_bundle_candidates(bundle_t const& bundle, std::vector<candidate_t>& candidates) const -> void
    {
        auto const& cluster = clusters_[bundle.applied_cluster_index];
        for (auto const candidate_index : bundle.candidate_indices)
        {
            auto& candidate = candidates[candidate_index];
            candidate.bundle_id = bundle.id;
            candidate.bundle_member_count = bundle.candidate_indices.size();
            candidate.bundle_rank_mask = bundle.rank_mask;
            candidate.bundle_horizon_mask = bundle.horizon_mask;
            candidate.bundle_representative_intercept_ns = bundle.representative_intercept_ns;
            candidate.bundle_saturation = bundle.saturation;
            candidate.bundle_saturated_witness = bundle.saturated_witness;
            candidate.bundle_target_cluster_id = cluster.id;
            candidate.bundle_independent_evidence = bundle.independent_evidence;
            candidate.bundle_action = bundle.action;
            candidate.operational_decision = bundle.operational_decision;
            candidate.cluster_relative_distance = bundle.assignment_relative_distance;
            candidate.cluster_center_ns = cluster.center_ns;
            candidate.cluster_median_absolute_deviation_ns = cluster.median_absolute_deviation_ns;
            candidate.cluster_independent_evidence_count = cluster.independent_periods_ns.size();
            candidate.cluster_supporting_rank_mask = cluster.supporting_rank_mask;
            candidate.cluster_supporting_horizon_mask = cluster.supporting_horizon_mask;
            candidate.cluster_independent_rank_mask = cluster.independent_supporting_rank_mask;
            candidate.cluster_independent_horizon_mask = cluster.independent_supporting_horizon_mask;
            candidate.cluster_maximum_independent_saturation = cluster.maximum_independent_saturation;
            candidate.cluster_saturated_witness_count = cluster.saturated_witness_count;
            candidate.cluster_maximum_observed_saturation = cluster.maximum_observed_saturation;
            candidate.cluster_density_contradicted = cluster.density_contradicted;
            candidate.cluster_recurring = cluster.recurring;
            candidate.cluster_identified = cluster.identified;
            candidate.cluster_identification_blocker = cluster_identification_blocker(cluster);
        }
    }

    auto update_holdover(long double reinforcement_elapsed_ns) -> void
    {
        if (last_reinforcement_elapsed_ns_ && reinforcement_elapsed_ns >= *last_reinforcement_elapsed_ns_)
            longest_holdover_ns_
                = std::max(longest_holdover_ns_, reinforcement_elapsed_ns - *last_reinforcement_elapsed_ns_);
        last_reinforcement_elapsed_ns_ = reinforcement_elapsed_ns;
    }

    auto finalize_holdover() -> void
    {
        if (!qualified_cluster_index_ || !last_reinforcement_elapsed_ns_) return;

        auto const end_elapsed_ns = completed_duration_ns_
            + (chain_first_timestamp_ns_ && last_timestamp_ns_
                    ? static_cast<long double>(*last_timestamp_ns_ - *chain_first_timestamp_ns_)
                    : 0.0L);

        if (end_elapsed_ns >= *last_reinforcement_elapsed_ns_)
            longest_holdover_ns_ = std::max(longest_holdover_ns_, end_elapsed_ns - *last_reinforcement_elapsed_ns_);
    }

    auto record_incomplete_windows(std::string const& reason) -> void
    {
        for (auto& window : windows_)
        {
            if (window.timestamps.empty()) continue;

            for (auto percentile_index = std::size_t{}; percentile_index < percentiles.size(); ++percentile_index)
            {
                auto candidate = candidate_t{};
                candidate.id = next_candidate_id_++;
                candidate.chain_id = chain_id_;
                candidate.begin_chain_report = window.begin_chain_report;
                candidate.end_chain_report = window.begin_chain_report + window.timestamps.size();
                candidate.begin_global_report = window.begin_global_report;
                candidate.end_global_report = window.begin_global_report + window.timestamps.size();
                candidate.begin_timestamp_ns = window.timestamps.front();
                candidate.end_timestamp_ns = window.timestamps.back();
                candidate.wall_duration_ns = candidate.end_timestamp_ns - candidate.begin_timestamp_ns;
                candidate.horizon = window.horizon;
                candidate.horizon_index = static_cast<std::size_t>(
                    std::find(horizons.begin(), horizons.end(), window.horizon) - horizons.begin());
                candidate.percentile_index = percentile_index;
                candidate.percentile = percentiles[percentile_index];
                candidate.structural_status = structural_status_t::incomplete_window;
                candidate.failure_reason = reason;
                candidate.operational_decision = operational_decision_t::structural_failure;
                candidate.availability_elapsed_ns = current_elapsed_ns(candidate.end_timestamp_ns);

                for (auto lag_index = std::size_t{}; lag_index < lags.size(); ++lag_index)
                {
                    auto const lag = lags[lag_index];
                    candidate.support[lag_index] = window.timestamps.size() > lag ? window.timestamps.size() - lag : 0;
                }

                ++structural_failure_candidate_rows_;
                ++incomplete_window_candidate_rows_;
                write_candidate(candidate);
            }

            window.timestamps.clear();
        }
    }

    auto finalize_isolated_lower_clusters() -> void
    {
        for (auto index = active_cluster_begin_; index < clusters_.size(); ++index)
        {
            auto& cluster = clusters_[index];
            if (!cluster.lower_than_qualified || cluster.isolated_lower_logged
                || cluster.independent_periods_ns.size() != 1)
            {
                continue;
            }

            cluster.isolated_lower_logged = true;
            ++rejected_isolated_lower_clusters_;
            append_event(event_kind_t::isolated_lower_outlier_rejected, cluster.id, cluster.center_ns,
                "one independent evidence bundle");
        }
    }

    auto emit_cluster_creation(cluster_t& cluster) -> void
    {
        if (cluster.creation_logged) return;

        cluster.creation_logged = true;
        append_event_at(event_kind_t::cluster_created, cluster.creation_elapsed_ns, cluster.creation_global_report,
            cluster.id, cluster.anchor_ns, {});
    }

    auto append_event(event_kind_t kind, std::optional<std::uint64_t> cluster_id, std::optional<long double> period_ns,
        std::string detail) -> void
    {
        append_event_at(kind, last_timestamp_ns_ ? current_elapsed_ns(*last_timestamp_ns_) : completed_duration_ns_,
            total_report_count_, cluster_id, period_ns, std::move(detail));
    }

    auto append_event_at(event_kind_t kind, long double elapsed_ns, std::uint64_t global_report,
        std::optional<std::uint64_t> cluster_id, std::optional<long double> period_ns, std::string detail) -> void
    {
        auto event = event_t{};
        event.kind = kind;
        event.elapsed_ns = elapsed_ns;
        event.global_report = global_report;
        event.cluster_id = cluster_id;
        event.period_ns = period_ns;
        event.detail = std::move(detail);
        events_.push_back(std::move(event));
    }

    static auto event_name(event_kind_t kind) -> std::string_view
    {
        switch (kind)
        {
            case event_kind_t::cluster_created: return "new cluster created";
            case event_kind_t::cluster_reinforced: return "cluster reinforced with independent evidence";
            case event_kind_t::cluster_became_recurring: return "cluster became recurring";
            case event_kind_t::cluster_identified: return "cluster identified by saturation";
            case event_kind_t::cluster_density_contradicted: return "cluster contradicted by visible density";
            case event_kind_t::cluster_qualified: return "cluster qualified";
            case event_kind_t::isolated_lower_outlier_rejected: return "candidate rejected as isolated lower outlier";
            case event_kind_t::higher_candidate_rejected_during_holdover:
                return "higher candidate rejected during holdover";
            case event_kind_t::recurring_lower_cluster_detected: return "recurring lower cluster detected";
            case event_kind_t::qualified_estimate_invalidated: return "qualified estimate invalidated";
            case event_kind_t::reacquisition_completed: return "reacquisition completed";
            case event_kind_t::observation_chain_broken: return "observation chain broken";
        }
        throw std::logic_error{"unknown replay event"};
    }

    static auto structural_status_name(structural_status_t status) -> std::string_view
    {
        switch (status)
        {
            case structural_status_t::accepted: return "accepted";
            case structural_status_t::incomplete_window: return "incomplete_window";
            case structural_status_t::insufficient_support: return "insufficient_support";
            case structural_status_t::invalid_fit: return "invalid_fit";
        }
        throw std::logic_error{"unknown structural status"};
    }

    static auto bundle_action_name(bundle_action_t action) -> std::string_view
    {
        switch (action)
        {
            case bundle_action_t::none: return "none";
            case bundle_action_t::created: return "created";
            case bundle_action_t::reinforced: return "reinforced";
            case bundle_action_t::corroborated: return "corroborated";
        }
        throw std::logic_error{"unknown bundle action"};
    }

    static auto operational_decision_name(operational_decision_t decision) -> std::string_view
    {
        switch (decision)
        {
            case operational_decision_t::none: return "none";
            case operational_decision_t::prequalification_evidence: return "prequalification_evidence";
            case operational_decision_t::recurring_unidentified: return "recurring_unidentified";
            case operational_decision_t::identification: return "identification";
            case operational_decision_t::qualified_cluster_update: return "qualified_cluster_update";
            case operational_decision_t::qualified_cluster_corroboration: return "qualified_cluster_corroboration";
            case operational_decision_t::identified_challenger: return "identified_challenger";
            case operational_decision_t::rejected_high_holdover: return "rejected_high_holdover";
            case operational_decision_t::lower_challenge: return "lower_challenge";
            case operational_decision_t::rejected_poor_fit: return "rejected_poor_fit";
            case operational_decision_t::structural_failure: return "structural_failure";
        }
        throw std::logic_error{"unknown operational decision"};
    }

    auto write_sidecar_header() -> void
    {
        *sidecar_ << "candidate_id\tcapture\tchain_id"
                  << "\tbegin_chain_report\tend_chain_report"
                  << "\tbegin_global_report\tend_global_report"
                  << "\tbegin_timestamp_ns\tend_timestamp_ns\twall_duration_ns"
                  << "\thorizon\tpercentile"
                  << "\tsupport_32\tsupport_64\tsupport_128\tsupport_256"
                  << "\tspan_32_ns\tspan_64_ns\tspan_128_ns\tspan_256_ns"
                  << "\tintercept_ns\tslope_ns_reports"
                  << "\tmean_squared_error_ns2\trmse_ns\tmax_abs_error_ns"
                  << "\tstructural_status\tfailure_reason\tcredible\tcandidate_saturation"
                  << "\tbundle_id\tbundle_member_count\tbundle_rank_mask\tbundle_horizon_mask"
                  << "\tbundle_representative_intercept_ns\tbundle_saturation"
                  << "\tbundle_saturated_witness\tbundle_target_cluster_id"
                  << "\tbundle_independent_evidence\tbundle_action\tbundle_operational_decision"
                  << "\tcluster_relative_distance\tcluster_center_ns\tcluster_mad_ns"
                  << "\tcluster_independent_evidence_count"
                  << "\tcluster_rank_mask\tcluster_horizon_mask"
                  << "\tcluster_independent_rank_mask\tcluster_independent_horizon_mask"
                  << "\tcluster_maximum_independent_saturation\tcluster_saturated_witness_count"
                  << "\tcluster_maximum_observed_saturation\tcluster_density_contradicted"
                  << "\tcluster_recurring\tcluster_identified\tcluster_identification_blocker\n";

        if (!*sidecar_) throw std::runtime_error{"failed while writing polling-period sidecar header"};
    }

    auto write_candidate(candidate_t const& candidate) -> void
    {
        if (!sidecar_) return;

        auto& output = *sidecar_;
        output << candidate.id << '\t' << sanitize_tsv(config_.capture_identity) << '\t' << candidate.chain_id << '\t'
               << candidate.begin_chain_report << '\t' << candidate.end_chain_report << '\t'
               << candidate.begin_global_report << '\t' << candidate.end_global_report << '\t'
               << candidate.begin_timestamp_ns << '\t' << candidate.end_timestamp_ns << '\t'
               << candidate.wall_duration_ns << '\t' << candidate.horizon << '\t' << candidate.percentile.name;

        for (auto const support : candidate.support) output << '\t' << support;

        for (auto const& span : candidate.selected_span_ns)
        {
            output << '\t';
            if (span) output << *span;
        }

        output << '\t';
        if (candidate.fit) output << candidate.fit->intercept_ns;
        output << '\t';
        if (candidate.fit) output << candidate.fit->slope_ns_reports;
        output << '\t';
        if (candidate.fit) output << candidate.fit->mean_squared_error_ns2;
        output << '\t';
        if (candidate.fit) output << std::sqrt(candidate.fit->mean_squared_error_ns2);
        output << '\t';
        if (candidate.fit) output << candidate.fit->maximum_absolute_error_ns;

        output << '\t' << structural_status_name(candidate.structural_status) << '\t'
               << sanitize_tsv(candidate.failure_reason) << '\t' << (candidate.credible ? 1 : 0) << '\t';
        if (candidate.candidate_saturation) output << *candidate.candidate_saturation;
        output << '\t';
        if (candidate.bundle_id) output << *candidate.bundle_id;
        output << '\t';
        if (candidate.bundle_member_count) output << *candidate.bundle_member_count;
        output << '\t';
        if (candidate.bundle_rank_mask) output << *candidate.bundle_rank_mask;
        output << '\t';
        if (candidate.bundle_horizon_mask) output << *candidate.bundle_horizon_mask;
        output << '\t';
        if (candidate.bundle_representative_intercept_ns) output << *candidate.bundle_representative_intercept_ns;
        output << '\t';
        if (candidate.bundle_saturation) output << *candidate.bundle_saturation;
        output << '\t' << (candidate.bundle_saturated_witness ? 1 : 0) << '\t';
        if (candidate.bundle_target_cluster_id) output << *candidate.bundle_target_cluster_id;
        output << '\t' << (candidate.bundle_independent_evidence ? 1 : 0) << '\t'
               << bundle_action_name(candidate.bundle_action) << '\t'
               << operational_decision_name(candidate.operational_decision) << '\t';
        if (candidate.cluster_relative_distance) output << *candidate.cluster_relative_distance;
        output << '\t';
        if (candidate.cluster_center_ns) output << *candidate.cluster_center_ns;
        output << '\t';
        if (candidate.cluster_median_absolute_deviation_ns) output << *candidate.cluster_median_absolute_deviation_ns;
        output << '\t';
        if (candidate.cluster_independent_evidence_count) output << *candidate.cluster_independent_evidence_count;
        output << '\t';
        if (candidate.cluster_supporting_rank_mask) output << *candidate.cluster_supporting_rank_mask;
        output << '\t';
        if (candidate.cluster_supporting_horizon_mask) output << *candidate.cluster_supporting_horizon_mask;
        output << '\t';
        if (candidate.cluster_independent_rank_mask) output << *candidate.cluster_independent_rank_mask;
        output << '\t';
        if (candidate.cluster_independent_horizon_mask) output << *candidate.cluster_independent_horizon_mask;
        output << '\t';
        if (candidate.cluster_maximum_independent_saturation)
            output << *candidate.cluster_maximum_independent_saturation;
        output << '\t';
        if (candidate.cluster_saturated_witness_count) output << *candidate.cluster_saturated_witness_count;
        output << '\t';
        if (candidate.cluster_maximum_observed_saturation) output << *candidate.cluster_maximum_observed_saturation;
        output << '\t';
        if (candidate.cluster_density_contradicted) output << (*candidate.cluster_density_contradicted ? 1 : 0);
        output << '\t';
        if (candidate.cluster_recurring) output << (*candidate.cluster_recurring ? 1 : 0);
        output << '\t';
        if (candidate.cluster_identified) output << (*candidate.cluster_identified ? 1 : 0);
        output << '\t' << sanitize_tsv(candidate.cluster_identification_blocker) << '\n';

        if (!output) throw std::runtime_error{"failed while writing polling-period candidate sidecar"};
    }

    config_t config_;
    std::vector<window_t> windows_{};
    std::optional<std::ofstream> sidecar_{};

    bool finished_{};
    std::uint64_t chain_id_{};
    std::uint64_t chain_report_count_{};
    std::uint64_t total_report_count_{};
    std::optional<std::uint64_t> chain_first_timestamp_ns_{};
    std::optional<std::uint64_t> last_timestamp_ns_{};
    long double completed_duration_ns_{};

    std::uint64_t next_candidate_id_{};
    std::uint64_t next_bundle_id_{};
    std::uint64_t next_cluster_id_{};
    std::vector<cluster_t> clusters_{};
    std::size_t active_cluster_begin_{};
    std::optional<std::size_t> qualified_cluster_index_{};
    std::optional<std::size_t> last_qualified_cluster_index_{};
    std::optional<long double> qualified_period_ns_{};
    std::optional<long double> cluster_center_at_qualification_ns_{};

    std::optional<std::uint64_t> first_candidate_report_{};
    std::optional<long double> first_candidate_elapsed_ns_{};
    std::optional<std::uint64_t> qualification_report_{};
    std::optional<long double> qualification_elapsed_ns_{};
    std::size_t qualification_independent_evidence_{};
    std::size_t qualification_saturated_witnesses_{};
    std::optional<long double> qualification_maximum_saturation_{};
    std::uint32_t qualification_cluster_rank_mask_{};
    std::uint32_t qualification_cluster_horizon_mask_{};
    std::uint32_t qualification_independent_rank_mask_{};
    std::uint32_t qualification_independent_horizon_mask_{};

    std::size_t credible_candidate_rows_{};
    std::size_t credible_evidence_intervals_{};
    std::size_t credible_evidence_bundles_{};
    std::size_t independent_qualified_cluster_bundles_{};
    std::size_t rejected_isolated_lower_clusters_{};
    std::size_t rejected_high_candidate_rows_{};
    std::size_t rejected_high_bundles_{};
    std::size_t poor_fit_candidate_rows_{};
    std::size_t structural_failure_candidate_rows_{};
    std::size_t incomplete_window_candidate_rows_{};
    std::size_t observation_chain_breaks_{};
    std::size_t invalidations_{};
    std::size_t cluster_switches_{};
    bool awaiting_reacquisition_{};

    std::optional<long double> last_reinforcement_elapsed_ns_{};
    long double longest_holdover_ns_{};
    std::vector<event_t> events_{};
};

polling_period_replay_t::polling_period_replay_t(config_t config) : impl_{std::make_unique<impl_t>(std::move(config))}
{}

polling_period_replay_t::~polling_period_replay_t() = default;

polling_period_replay_t::polling_period_replay_t(polling_period_replay_t&&) noexcept = default;

auto polling_period_replay_t::operator=(polling_period_replay_t&&) noexcept -> polling_period_replay_t& = default;

auto polling_period_replay_t::observe(std::uint64_t timestamp_ns) -> void
{
    impl_->observe(timestamp_ns);
}

auto polling_period_replay_t::break_observation_chain(std::string_view reason) -> void
{
    impl_->break_observation_chain(reason);
}

auto polling_period_replay_t::reset(std::string_view reason) -> void
{
    impl_->reset(reason);
}

auto polling_period_replay_t::finish() -> void
{
    impl_->finish();
}

auto polling_period_replay_t::print(std::ostream& stream) const -> void
{
    impl_->print(stream);
}

auto polling_period_replay_t::summary() const -> summary_t
{
    return impl_->make_summary();
}

} // namespace crv
