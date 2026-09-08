// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "refinement_pool_seeder.hpp"
#include <crv/spline/construction/error.hpp>
#include <crv/spline/construction/segment/amr/interval.hpp>
#include <crv/spline/construction/spline/amr/seed/subdomain_factory.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

namespace crv::spline {
namespace {

struct spline_refinement_pool_seeder_test_t : Test
{
    using x_t = fixed_t<int_t, 8>;
    using scalar_t = float_t;
    using subdomain_t = spline::subdomain_t<scalar_t, x_t>;
    using jet_t = subdomain_t::jet_t;
    using function_sample_t = subdomain_t::function_sample_t;

    struct refinement_pool_t : std::vector<subdomain_t>
    {
        constexpr auto emplace(subdomain_t subdomain) -> void { this->push_back(std::move(subdomain)); }
    };
    struct workspace_t
    {
        refinement_pool_t refinement_pool;
    };
    workspace_t workspace;

    struct common_typestate_t
    {
        workspace_t& workspace;
    };

    struct next_typestate_t : common_typestate_t
    {};

    struct typestate_t : common_typestate_t
    {
        using next_t = next_typestate_t;
    };

    using subdomain_factory_t = seed::subdomain_factory_t<x_t, subdomain_t>;

    struct interval_factory_t
    {
        using error_t = spline_construction_error_t<x_t>;

        std::optional<x_t> fail_left_x;

        auto operator()(auto const&, subdomain_t const& subdomain) const noexcept -> std::expected<subdomain_t, error_t>
        {
            if (fail_left_x == subdomain.left_x)
            {
                return std::unexpected{error_t{
                    .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
                    .left = subdomain.left_x,
                    .right = subdomain.right_x,
                }};
            }
            return subdomain;
        }
    };

    static constexpr auto max_segment_count = 3;
    static constexpr auto log2_domain_end = 4;
    using sut_t = refinement_pool_seeder_t<typestate_t, subdomain_factory_t, interval_factory_t, max_segment_count,
        log2_domain_end>;
    using critical_points_t = sut_t::critical_points_t;

    sut_t sut{};

    struct target_t
    {
        constexpr auto transfer(jet_t input) const noexcept -> jet_t { return {input.f * 2.0, input.df * 2.0}; }
    };
    static constexpr auto target = target_t{};
};

TEST_F(spline_refinement_pool_seeder_test_t, seeds_one_interval_between_each_exact_supplied_knot)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(102)};

    auto const actual_state = sut(typestate_t{workspace}, target, critical_points);

    ASSERT_TRUE(actual_state);
    ASSERT_EQ(workspace.refinement_pool.size(), 3u);
    EXPECT_EQ(workspace.refinement_pool[0].left_x, x_t{0});
    EXPECT_EQ(workspace.refinement_pool[0].right_x, critical_points[0]);
    EXPECT_EQ(workspace.refinement_pool[1].left_x, critical_points[0]);
    EXPECT_EQ(workspace.refinement_pool[1].right_x, critical_points[1]);
    EXPECT_EQ(workspace.refinement_pool[2].left_x, critical_points[1]);
    EXPECT_EQ(workspace.refinement_pool[2].right_x, sut_t::domain_end);
    EXPECT_EQ(&actual_state->workspace, &workspace);
}

TEST_F(spline_refinement_pool_seeder_test_t, interval_construction_failure_is_propagated_without_continuing)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(102)};
    sut.create_interval.fail_left_x = critical_points[0];

    auto const result = sut(typestate_t{workspace}, target, critical_points);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(),
        (interval_factory_t::error_t{
            .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
            .left = critical_points[0],
            .right = critical_points[1],
        }));
    ASSERT_EQ(workspace.refinement_pool.size(), 1u);
    EXPECT_EQ(workspace.refinement_pool.front().left_x, x_t{0});
    EXPECT_EQ(workspace.refinement_pool.front().right_x, critical_points[0]);
}

TEST_F(spline_refinement_pool_seeder_test_t, permits_supplied_knots_one_raw_unit_apart)
{
    auto const critical_points = critical_points_t{x_t::literal(100), x_t::literal(101)};

    auto const result = sut(typestate_t{workspace}, target, critical_points);

    ASSERT_TRUE(result);
    ASSERT_EQ(workspace.refinement_pool.size(), 3u);
    EXPECT_EQ(workspace.refinement_pool[1].width(), x_t::literal(1));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_unique)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(101)};
    EXPECT_DEBUG_DEATH(static_cast<void>(sut(typestate_t{workspace}, target, critical_points)), "unique");
}

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_monotonically_increasing)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(100)};
    EXPECT_DEBUG_DEATH(
        static_cast<void>(sut(typestate_t{workspace}, target, critical_points)), "monotonically increasing");
}

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_inside_domain)
{
    EXPECT_DEBUG_DEATH(
        static_cast<void>(sut(typestate_t{workspace}, target, critical_points_t{x_t{0}})), "in \\(0, domain_end\\)");
}

TEST_F(spline_refinement_pool_seeder_test_t, critical_point_partitioning_must_fit_segment_budget)
{
    auto const critical_points = critical_points_t{x_t::literal(100), x_t::literal(101), x_t::literal(102)};
    EXPECT_DEBUG_DEATH(
        static_cast<void>(sut(typestate_t{workspace}, target, critical_points)), "exceeded segment budget");
}

TEST_F(spline_refinement_pool_seeder_test_t, refinement_pool_must_be_empty)
{
    workspace.refinement_pool.push_back({});
    EXPECT_DEBUG_DEATH(static_cast<void>(sut(typestate_t{workspace}, target, {})), "empty");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
