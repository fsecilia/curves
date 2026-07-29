// SPDX-License-Identifier: MIT

/// \file
/// \brief exact chronological reference estimator for visible mouse polling periods

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace crv {

class polling_period_replay_t final
{
public:
    struct config_t
    {
        std::string capture_identity{"capture"};
        long double cluster_relative_tolerance{0.005L};
        long double max_relative_fit_rmse{0.0025L};

        // A cluster may recur without being operationally identified. The
        // default identification rule requires two disjoint evidence bundles,
        // at least one near-saturated witness, and no density contradiction.
        std::size_t qualification_independent_evidence{2};
        std::size_t qualification_saturated_witnesses{1};
        long double minimum_saturation{0.90L};
        long double maximum_sustainable_saturation{1.02L};

        std::optional<std::filesystem::path> sidecar_path{};
    };

    struct summary_t
    {
        std::uint64_t total_reports{};
        long double duration_seconds{};

        bool qualified{};
        std::optional<long double> qualified_period_ns{};
        std::optional<long double> cluster_center_at_qualification_ns{};
        std::optional<long double> final_qualified_cluster_center_ns{};
        std::optional<std::uint64_t> first_candidate_report{};
        std::optional<long double> first_candidate_seconds{};
        std::optional<std::uint64_t> qualification_report{};
        std::optional<long double> qualification_seconds{};
        std::size_t qualification_independent_evidence{};
        std::size_t qualification_saturated_witnesses{};
        std::optional<long double> qualification_maximum_saturation{};
        std::uint32_t qualification_cluster_rank_mask{};
        std::uint32_t qualification_cluster_horizon_mask{};
        std::uint32_t qualification_independent_rank_mask{};
        std::uint32_t qualification_independent_horizon_mask{};

        std::size_t credible_candidate_rows{};
        std::size_t credible_evidence_intervals{};
        std::size_t credible_evidence_bundles{};
        std::size_t recurring_clusters{};
        std::size_t identified_clusters{};
        std::size_t recurring_unidentified_clusters{};
        std::size_t density_contradicted_clusters{};
        std::size_t independent_qualified_cluster_bundles{};
        std::size_t rejected_isolated_lower_clusters{};
        std::size_t rejected_high_candidate_rows{};
        std::size_t rejected_high_bundles{};
        std::size_t poor_fit_candidate_rows{};
        std::size_t structural_failure_candidate_rows{};
        std::size_t incomplete_window_candidate_rows{};
        std::size_t observation_chain_breaks{};
        long double longest_holdover_seconds{};

        std::optional<long double> qualified_cluster_minimum_ns{};
        std::optional<long double> qualified_cluster_median_ns{};
        std::optional<long double> qualified_cluster_maximum_ns{};
        std::optional<long double> qualified_cluster_maximum_independent_saturation{};
        std::size_t qualified_cluster_saturated_witnesses{};
        bool qualified_cluster_density_contradicted{};

        std::size_t invalidations{};
        std::size_t cluster_switches{};

        // Compatibility aliases for the first draft's summary API.
        std::size_t accepted_updates{};
        std::size_t rejected_high_candidates{};
    };

    explicit polling_period_replay_t(config_t config);
    ~polling_period_replay_t();

    polling_period_replay_t(polling_period_replay_t&&) noexcept;
    auto operator=(polling_period_replay_t&&) noexcept -> polling_period_replay_t&;

    polling_period_replay_t(polling_period_replay_t const&) = delete;
    auto operator=(polling_period_replay_t const&) -> polling_period_replay_t& = delete;

    /// Observes one valid visible-report timestamp.
    ///
    /// Timestamps must be monotonic within an observation chain. Ordinary idle
    /// gaps do not end a chain.
    auto observe(std::uint64_t timestamp_ns) -> void;

    /// Ends span/window continuity while preserving clusters and qualification.
    ///
    /// Use this for a known loss of captured reports or another discontinuity
    /// that makes report-count spans across the boundary invalid.
    auto break_observation_chain(std::string_view reason) -> void;

    /// Ends the current chain and begins a fresh acquisition.
    ///
    /// Use this for device replacement, selected-device changes, or a timestamp
    /// clock-domain reset.
    auto reset(std::string_view reason) -> void;

    /// Records incomplete windows, finalizes holdover accounting, and flushes diagnostics.
    auto finish() -> void;

    /// Prints the compact replay summary and meaningful transition log.
    /// finish() must have completed successfully first.
    auto print(std::ostream& stream) const -> void;

    [[nodiscard]] auto summary() const -> summary_t;

private:
    class impl_t;
    std::unique_ptr<impl_t> impl_;
};

} // namespace crv
