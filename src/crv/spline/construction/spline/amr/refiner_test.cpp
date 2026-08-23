// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "refiner.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <optional>
#include <queue>
#include <vector>

namespace crv::spline {
namespace {

struct spline_refiner_test_t : Test
{
    using x_t = fixed_t<int_t, 0>;

    struct subdomain_t
    {
        using x_t = spline_refiner_test_t::x_t;

        x_t left_x;
        x_t midpoint_x;
        x_t right_x;

        constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
    };

    struct interval_t
    {
        using subdomain_t = spline_refiner_test_t::subdomain_t;

        int_t id;
        subdomain_t subdomain;
        std::optional<int_t> residual;

        constexpr auto operator<(interval_t const& rhs) const noexcept -> bool { return id < rhs.id; }
        constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    static constexpr auto safe(int_t id, x_t left = x_t{0}, x_t midpoint = x_t{1}, x_t right = x_t{2}) -> interval_t
    {
        return {.id = id, .subdomain = {left, midpoint, right}, .residual = 0};
    }

    static constexpr auto unsafe(int_t id, x_t left = x_t{0}, x_t midpoint = x_t{1}, x_t right = x_t{2}) -> interval_t
    {
        return {.id = id, .subdomain = {left, midpoint, right}, .residual = std::nullopt};
    }

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

    struct typestate_t
    {
        workspace_t& workspace;
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

TEST_F(spline_refiner_test_t, safe_complete_interval_finishes)
{
    workspace.refinement_pool.push(safe(1));
    EXPECT_CALL(mock_requires_subdivision, call(safe(1))).WillOnce(Return(false));

    auto const result = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_TRUE(result);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{safe(1)}));
}

TEST_F(spline_refiner_test_t, safe_full_pool_can_complete)
{
    for (auto id = int_t{1}; id <= max_segment_count; ++id) workspace.refinement_pool.push(safe(id));
    for (auto id = max_segment_count; id >= 1; --id)
        EXPECT_CALL(mock_requires_subdivision, call(safe(id))).WillOnce(Return(false));

    auto const result = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_TRUE(result);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{safe(4), safe(3), safe(2), safe(1)}));
}

TEST_F(spline_refiner_test_t, unsafe_interval_forces_subdivision_without_quality_evaluation)
{
    workspace.refinement_pool.push(unsafe(10));

    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), unsafe(10)))
        .WillOnce(Return(subdivision_t{safe(20), safe(30)}));
    EXPECT_CALL(mock_requires_subdivision, call(safe(30))).WillOnce(Return(false));
    EXPECT_CALL(mock_requires_subdivision, call(safe(20))).WillOnce(Return(false));

    auto const result = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_TRUE(result);
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{safe(30), safe(20)}));
}

TEST_F(spline_refiner_test_t, optional_refinement_stops_at_combined_segment_budget)
{
    workspace.refinement_pool.push(safe(10));
    workspace.refinement_pool.push(safe(20));

    EXPECT_CALL(mock_requires_subdivision, call(safe(20))).WillOnce(Return(true));
    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), safe(20)))
        .WillOnce(Return(subdivision_t{safe(40), safe(30)}));
    EXPECT_CALL(mock_requires_subdivision, call(safe(40))).WillOnce(Return(false));
    EXPECT_CALL(mock_requires_subdivision, call(safe(30))).WillOnce(Return(true));
    EXPECT_CALL(mock_subdivide, call(Ref(sample_target_function), safe(30)))
        .WillOnce(Return(subdivision_t{safe(50), safe(60)}));
    EXPECT_CALL(mock_requires_subdivision, call(safe(60))).WillOnce(Return(true));

    auto const result = sut(typestate_t{workspace}, sample_target_function);

    EXPECT_TRUE(result);
    EXPECT_TRUE(workspace.refinement_pool.empty());
    EXPECT_EQ(workspace.completed_intervals, (intervals_t{safe(40), safe(60), safe(50), safe(10)}));
}

TEST_F(spline_refiner_test_t, required_refinement_fails_at_segment_budget_with_exact_range)
{
    workspace.refinement_pool.push(safe(1));
    workspace.refinement_pool.push(safe(2));
    workspace.refinement_pool.push(safe(3));
    workspace.refinement_pool.push(unsafe(4, x_t{11}, x_t{12}, x_t{15}));

    auto const result = sut(typestate_t{workspace}, sample_target_function);

    ASSERT_FALSE(result);
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->reason, spline_generation_error_reason_t::segment_budget_exhausted);
    EXPECT_EQ(result.error->left, x_t{11});
    EXPECT_EQ(result.error->right, x_t{15});
}

TEST_F(spline_refiner_test_t, required_refinement_fails_when_no_distinct_midpoint_exists)
{
    workspace.refinement_pool.push(unsafe(1, x_t{7}, x_t{7}, x_t{8}));

    auto const result = sut(typestate_t{workspace}, sample_target_function);

    ASSERT_FALSE(result);
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->reason, spline_generation_error_reason_t::minimum_interval_width);
    EXPECT_EQ(result.error->left, x_t{7});
    EXPECT_EQ(result.error->right, x_t{8});
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_refiner_test_t, asserts_on_empty_refinement_pool)
{
    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function), "must not be empty");
}

TEST_F(spline_refiner_test_t, asserts_on_overfull_refinement_pool)
{
    for (auto id = int_t{1}; id <= max_segment_count + 1; ++id) workspace.refinement_pool.push(safe(id));
    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function), "overfull");
}

TEST_F(spline_refiner_test_t, asserts_on_non_empty_completed_intervals)
{
    workspace.refinement_pool.push(safe(1));
    workspace.completed_intervals.push_back(safe(2));
    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function), "must be empty");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
