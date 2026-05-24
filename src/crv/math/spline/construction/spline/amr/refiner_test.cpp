// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "refiner.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <queue>
#include <vector>

namespace crv::spline {
namespace {

struct spline_refiner_test_t : Test
{
    struct interval_t
    {
        int_t id;

        constexpr auto operator<=>(interval_t const&) const noexcept -> auto = default;
        constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    struct subdivision_t
    {
        interval_t left;
        interval_t right;
    };

    using intervals_t = std::vector<interval_t>;

    struct workspace_t
    {
        std::priority_queue<interval_t> refinement_pool;
        intervals_t completed_intervals;
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

    struct sample_target_function_t
    {};
    sample_target_function_t sample_target_function{};

    struct mock_subdivision_predicate_t
    {
        virtual ~mock_subdivision_predicate_t() = default;
        MOCK_METHOD(bool, call, (interval_t const&), (const, noexcept));
    };
    StrictMock<mock_subdivision_predicate_t> mock_requires_subdivision;

    struct subdivision_predicate_t
    {
        mock_subdivision_predicate_t* mock = nullptr;
        auto operator()(interval_t const& interval) const noexcept -> bool { return mock->call(interval); }
    };

    struct mock_subdivider_t
    {
        virtual ~mock_subdivider_t() = default;
        MOCK_METHOD(subdivision_t, call, (sample_target_function_t const&, interval_t const&), (const, noexcept));
    };
    StrictMock<mock_subdivider_t> mock_subdivide;

    struct subdivider_t
    {
        using interval_t = spline_refiner_test_t::interval_t;
        mock_subdivider_t* mock = nullptr;

        auto operator()(auto const& sample_target_function, interval_t const& interval) const noexcept -> subdivision_t
        {
            return mock->call(sample_target_function, interval);
        }
    };

    static constexpr int_t max_segment_count = 4;
    using sut_t = refiner_t<typestate_t, subdivider_t, subdivision_predicate_t, max_segment_count>;

    sut_t sut{.requires_subdivision = subdivision_predicate_t{&mock_requires_subdivision},
        .subdivide = subdivider_t{&mock_subdivide}};
};

TEST_F(spline_refiner_test_t, no_subdivision_required_drains_immediately)
{
    // start with 1 interval
    workspace.refinement_pool.push(interval_t{1});

    // predicate consulted; return false - loop not entered
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{1})).WillOnce(Return(false));

    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_EQ(&workspace, &actual.workspace);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{{1}}));
}

TEST_F(spline_refiner_test_t, refinement_pool_starts_full)
{
    // start with max_segment_count intervals
    workspace.refinement_pool.push(interval_t{1});
    workspace.refinement_pool.push(interval_t{2});
    workspace.refinement_pool.push(interval_t{3});
    workspace.refinement_pool.push(interval_t{4});

    // predicate isn't consulted - loop not entered

    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_EQ(&workspace, &actual.workspace);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{{4}, {3}, {2}, {1}}));
}

TEST_F(spline_refiner_test_t, refinement_pool_empties_before_budget_hit)
{
    // start with 1 interval
    // subdivide just a few, then stop and let main loop drain pool
    workspace.refinement_pool.push({10});

    // pool={10}, completed={}; pop 10, push {20, 30}
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{10})).WillOnce(Return(true));
    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), interval_t{10}))
        .WillOnce(Return(subdivision_t{interval_t{20}, interval_t{30}}));

    // pool={30, 20}, completed={}; pop 30, push {40, 50}
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{30})).WillOnce(Return(true));
    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), interval_t{30}))
        .WillOnce(Return(subdivision_t{interval_t{50}, interval_t{40}}));

    // pool={50, 40, 20}, completed={}; stop subdividing and let main loop drain pool
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{50})).WillOnce(Return(false));
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{40})).WillOnce(Return(false));
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{20})).WillOnce(Return(false));

    // pool={}, completed={50, 40, 20}; pool is empty so main loop stops
    // drain_remaining is called but has no work to do
    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_EQ(&workspace, &actual.workspace);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{{50}, {40}, {20}}));
}

TEST_F(spline_refiner_test_t, subdivision_hits_combined_max_segment_budget_and_drains)
{
    // start with multiple intervals
    // subdivide until combined budget hit
    workspace.refinement_pool.push({10});
    workspace.refinement_pool.push({20});

    // pool={20, 10}, completed={}; pop 20, push {40, 30}
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{20})).WillOnce(Return(true));
    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), interval_t{20}))
        .WillOnce(Return(subdivision_t{interval_t{40}, interval_t{30}}));

    // pool={40, 30, 10}, completed={}; decide interval is complete, moves 40 to completed
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{40})).WillOnce(Return(false));

    // pool={30, 10}, completed={40}; loop runs again, pop 30, push{50, 60}
    // subdivision budget hit
    EXPECT_CALL(mock_requires_subdivision, call(interval_t{30})).WillOnce(Return(true));
    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), interval_t{30}))
        .WillOnce(Return(subdivision_t{interval_t{50}, interval_t{60}}));

    // pool={60, 50, 10}, completed={40}; loop exits
    // drain_remaining sweeps remaining
    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_EQ(&workspace, &actual.workspace);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{{40}, {60}, {50}, {10}}));
}

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_refiner_test_t, asserts_on_empty_refinement_pool)
{
    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function), "must not be empty");
}

TEST_F(spline_refiner_test_t, asserts_on_overfull_refinement_pool)
{
    workspace.refinement_pool.push(interval_t{1});
    workspace.refinement_pool.push(interval_t{2});
    workspace.refinement_pool.push(interval_t{3});
    workspace.refinement_pool.push(interval_t{4});
    workspace.refinement_pool.push(interval_t{5});

    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function), "overfull");
}

TEST_F(spline_refiner_test_t, asserts_on_non_empty_completed_intervals)
{
    workspace.refinement_pool.push(interval_t{1});
    workspace.completed_intervals.push_back(interval_t{2});

    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function), "must be empty");
}

#endif

} // namespace
} // namespace crv::spline
