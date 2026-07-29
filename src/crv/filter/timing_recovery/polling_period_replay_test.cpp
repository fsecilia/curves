#include "polling_period_replay.hpp"
#include <crv/test/test.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

[[maybe_unused]] auto near(long double lhs, long double rhs, long double tolerance) -> bool
{
    return std::abs(lhs - rhs) <= tolerance;
}

auto observe_exact(crv::polling_period_replay_t& replay, std::uint64_t& timestamp, int count, std::uint64_t period_ns)
    -> void
{
    for (auto index = 0; index < count; ++index)
    {
        replay.observe(timestamp);
        timestamp += period_ns;
    }
}

auto observe_low_occupancy_window(crv::polling_period_replay_t& replay, std::uint64_t& timestamp) -> void
{
    for (auto index = 0; index < 512; ++index)
    {
        replay.observe(timestamp);
        timestamp += 250'000;
        if (index == 400) timestamp += 100'000'000;
    }
}

} // namespace

int main()
{
    {
        auto replay = crv::polling_period_replay_t{{
            .capture_identity = "exact-4k",
            .sidecar_path = "exact-4k-v3.tsv",
        }};

        auto timestamp = std::uint64_t{};
        observe_exact(replay, timestamp, 32'768, 250'000);
        replay.finish();

        [[maybe_unused]] auto const summary = replay.summary();
        assert(summary.qualified);
        assert(summary.qualified_period_ns);
        assert(summary.cluster_center_at_qualification_ns);
        assert(summary.final_qualified_cluster_center_ns);
        assert(near(*summary.qualified_period_ns, 250'000.0L, 0.01L));
        assert(near(*summary.final_qualified_cluster_center_ns, 250'000.0L, 0.01L));
        assert(summary.qualification_report == 1'024);
        assert(summary.qualification_independent_evidence == 2);
        assert(summary.qualification_saturated_witnesses == 2);
        assert(summary.qualification_maximum_saturation);
        assert(near(*summary.qualification_maximum_saturation, 1.0L, 0.0001L));
        assert(summary.qualification_independent_rank_mask == 0b1110U);
        assert(summary.qualification_independent_horizon_mask == 0b00001U);
        assert(summary.identified_clusters == 1);
        assert(summary.recurring_unidentified_clusters == 0);
        replay.print(std::cout);
    }

    {
        auto replay = crv::polling_period_replay_t{{
            .capture_identity = "recurring-but-unsaturated",
        }};

        auto timestamp = std::uint64_t{};
        for (auto window = 0; window < 4; ++window) observe_low_occupancy_window(replay, timestamp);
        replay.finish();

        [[maybe_unused]] auto const summary = replay.summary();
        assert(!summary.qualified);
        assert(summary.recurring_clusters >= 1);
        assert(summary.recurring_unidentified_clusters >= 1);
        assert(summary.identified_clusters == 0);
    }

    {
        auto replay = crv::polling_period_replay_t{{
            .capture_identity = "identified-challenger-does-not-switch-yet",
        }};

        auto timestamp = std::uint64_t{};
        observe_exact(replay, timestamp, 4'096, 250'000);
        observe_exact(replay, timestamp, 4'096, 255'000);
        replay.finish();

        [[maybe_unused]] auto const summary = replay.summary();
        assert(summary.qualified);
        assert(summary.qualified_period_ns);
        assert(near(*summary.qualified_period_ns, 250'000.0L, 0.01L));
        assert(summary.identified_clusters >= 2);
        assert(summary.cluster_switches == 0);
    }

    {
        auto replay = crv::polling_period_replay_t{{
            .capture_identity = "soft-break-preserves-qualification",
        }};

        auto timestamp = std::uint64_t{};
        observe_exact(replay, timestamp, 4'096, 1'000'000);
        replay.break_observation_chain("capture sequence gap");
        timestamp += 100'000'000;
        observe_exact(replay, timestamp, 4'096, 1'000'000);
        replay.finish();

        [[maybe_unused]] auto const summary = replay.summary();
        assert(summary.qualified);
        assert(summary.qualified_period_ns);
        assert(near(*summary.qualified_period_ns, 1'000'000.0L, 0.01L));
        assert(summary.observation_chain_breaks == 1);
        assert(summary.invalidations == 0);
    }

    {
        auto replay = crv::polling_period_replay_t{{
            .capture_identity = "density-contradiction",
        }};

        auto timestamp = std::uint64_t{};
        observe_exact(replay, timestamp, 2'048, 1'000'000);
        observe_exact(replay, timestamp, 1'024, 250'000);
        replay.finish();

        [[maybe_unused]] auto const summary = replay.summary();
        assert(summary.qualified);
        assert(summary.qualified_cluster_density_contradicted);
        assert(summary.density_contradicted_clusters >= 1);
    }

    {
        auto replay = crv::polling_period_replay_t{{
            .capture_identity = "reset-without-reacquisition",
        }};

        auto timestamp = std::uint64_t{};
        observe_exact(replay, timestamp, 4'096, 1'000'000);
        replay.reset("device changed");
        replay.finish();

        [[maybe_unused]] auto const summary = replay.summary();
        assert(!summary.qualified);
        assert(!summary.qualified_period_ns);
        assert(!summary.qualification_report);
        assert(summary.invalidations == 1);
    }
}
