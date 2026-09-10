// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "assembler.hpp"
#include <crv/spline/construction/error.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <expected>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using x_t = fixed_t<int_t, 8>;

struct segment_t
{
    using x_t = x_t;
    int_t payload_id{};

    constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
};

struct interval_t
{
    using segment_t = segment_t;

    struct subdomain_t
    {
        x_t left_x{};

        constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
    };
    subdomain_t subdomain;

    segment_t segment{};

    constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
};

//
// dependencies
//

namespace dependency_tests {

constexpr auto test_interval_sorter() noexcept -> bool
{
    auto actual = std::array<interval_t, 3>{{
        {.subdomain = {.left_x = x_t{3}}, .segment = {.payload_id = 99}},
        {.subdomain = {.left_x = x_t{1}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left_x = x_t{2}}, .segment = {.payload_id = 73}},
    }};

    interval_sorter_t{}(actual);

    auto const expected = std::array<interval_t, 3>{{
        {.subdomain = {.left_x = x_t{1}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left_x = x_t{2}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left_x = x_t{3}}, .segment = {.payload_id = 99}},
    }};

    return expected == actual;
}
static_assert(test_interval_sorter());

constexpr auto test_segment_projector_single_interval() noexcept -> bool
{
    auto const intervals
        = std::array<interval_t, 1>{{{.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 37}}}};
    auto actual = std::array<segment_t, 1>{};

    segment_projector_t{}(intervals, actual);

    return actual[0] == intervals[0].segment;
}
static_assert(test_segment_projector_single_interval());

constexpr auto test_segment_projector_multiple_intervals() noexcept -> bool
{
    auto const intervals = std::array<interval_t, 3>{{
        {.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left_x = x_t{20}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left_x = x_t{30}}, .segment = {.payload_id = 99}},
    }};
    auto actual = std::array<segment_t, 4>{{
        {.payload_id = 1},
        {.payload_id = 2},
        {.payload_id = 3},
        {.payload_id = 101},
    }};

    segment_projector_t{}(intervals, actual);

    auto const expected
        = std::array{intervals[0].segment, intervals[1].segment, intervals[2].segment, segment_t{.payload_id = 101}};
    return actual == expected;
}
static_assert(test_segment_projector_multiple_intervals());

constexpr auto test_locator_key_preparer() noexcept -> bool
{
    auto const intervals = std::array<interval_t, 3>{{
        {.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left_x = x_t{20}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left_x = x_t{30}}, .segment = {.payload_id = 99}},
    }};
    auto actual = std::array<x_t, 5>{};
    auto const x_max = x_t{100};

    locator_key_preparer_t{}(intervals, actual, x_max);

    auto const expected = std::array{x_t{20}, x_t{30}, x_max, x_max, x_max};
    return actual == expected;
}
static_assert(test_locator_key_preparer());

constexpr auto test_locator_key_preparer_single_interval() noexcept -> bool
{
    auto const intervals
        = std::array<interval_t, 1>{{{.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 37}}}};
    auto actual = std::array<x_t, 3>{};
    auto const x_max = x_t{100};

    locator_key_preparer_t{}(intervals, actual, x_max);

    auto const expected = std::array{x_t{100}, x_t{100}, x_t{100}};
    return actual == expected;
}
static_assert(test_locator_key_preparer_single_interval());

} // namespace dependency_tests

//
// assembler_t
//

namespace assembler_tests {

struct segment_locator_t
{
    static constexpr auto total_key_count = 2;
    static constexpr auto max_segment_count = 3;

    std::array<x_t, total_key_count> keys{};
    x_t max_x{};
    int_t count{};

    constexpr segment_locator_t() = default;
    constexpr segment_locator_t(auto const& k, auto mx, int_t c) : keys(k), max_x(mx), count(c) {}

    constexpr auto operator==(segment_locator_t const&) const noexcept -> bool = default;
};

struct spline_t
{
    using segment_locator_t = segment_locator_t;
    static constexpr auto max_segment_count = segment_locator_t::max_segment_count;

    std::array<segment_t, max_segment_count> segments{};
    segment_locator_t segment_locator{};
    int_t extend_final_tangent{};

    constexpr auto operator==(spline_t const&) const noexcept -> bool = default;
};

struct workspace_t
{
    std::vector<interval_t> completed_intervals;
};

struct typestate_t
{
    workspace_t& workspace;
};

struct tangent_extender_t
{
    using error_t = spline_construction_error_t<x_t>;
    using result_t = std::expected<int_t, error_t>;

    error_t const* failure = nullptr;

    constexpr auto operator()(interval_t const& interval) const noexcept -> result_t
    {
        if (failure) return std::unexpected{*failure};
        return interval.segment.payload_id;
    }
};

struct assembler_preparation_order_test_t : Test
{
    enum class event_t
    {
        sort,
        extend_tangent,
        project_segments,
        prepare_locator_keys,
    };

    using events_t = std::vector<event_t>;
    events_t events;

    struct interval_sorter_t
    {
        events_t* events;

        auto operator()(auto& intervals) const noexcept -> void
        {
            events->push_back(event_t::sort);
            crv::spline::interval_sorter_t{}(intervals);
        }
    };

    struct tangent_extender_t
    {
        using error_t = spline_construction_error_t<x_t>;
        using result_t = std::expected<int_t, error_t>;

        events_t* events;

        auto operator()(interval_t const& interval) const noexcept -> result_t
        {
            events->push_back(event_t::extend_tangent);
            return interval.segment.payload_id;
        }
    };

    struct segment_projector_t
    {
        events_t* events;

        auto operator()(auto const& intervals, auto& segments) const noexcept -> void
        {
            events->push_back(event_t::project_segments);
            crv::spline::segment_projector_t{}(intervals, segments);
        }
    };

    struct locator_key_preparer_t
    {
        events_t* events;

        auto operator()(auto const& intervals, auto& keys, auto const& x_max) const noexcept -> void
        {
            events->push_back(event_t::prepare_locator_keys);
            crv::spline::locator_key_preparer_t{}(intervals, keys, x_max);
        }
    };
};

TEST_F(assembler_preparation_order_test_t, prepares_before_destination_writes)
{
    auto workspace = workspace_t{};
    auto state = typestate_t{workspace};
    state.workspace.completed_intervals = {
        {.subdomain = {.left_x = x_t{20}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 42}},
    };
    auto spline = spline_t{};

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, segment_projector_t, locator_key_preparer_t,
        tangent_extender_t, 100>;
    auto const sut = sut_t{
        .sort_intervals = {&events},
        .project_segments = {&events},
        .prepare_locator_keys = {&events},
        .extend_tangent = {&events},
    };

    ASSERT_TRUE(sut(std::move(state), spline));

    EXPECT_EQ(events,
        (events_t{event_t::sort, event_t::prepare_locator_keys, event_t::extend_tangent, event_t::project_segments}));
}

TEST(spline_assembler_test, vs_real_dependencies)
{
    auto workspace = workspace_t{};
    auto state = typestate_t{workspace};

    // initialized out-of-order to prove the sorter runs in the pipeline
    state.workspace.completed_intervals = {
        {.subdomain = {.left_x = x_t{20}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 42}},
    };

    auto spline = spline_t{};

    constexpr auto domain_end_value = 100;

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, segment_projector_t, locator_key_preparer_t,
        tangent_extender_t, domain_end_value>;
    auto const sut = sut_t{};
    ASSERT_TRUE(sut(std::move(state), spline));

    // workspace must be clear
    EXPECT_TRUE(state.workspace.completed_intervals.empty());

    // payload must be sorted
    EXPECT_EQ(spline.segments[0].payload_id, 42);
    EXPECT_EQ(spline.segments[1].payload_id, 73);

    // locator must be populated
    auto const& locator = spline.segment_locator;
    EXPECT_EQ(locator.count, 2);
    EXPECT_EQ(locator.max_x, to_fixed<x_t>(100.0));

    // keys must contain left bounds of next segment, padded with max
    EXPECT_EQ(locator.keys[0], to_fixed<x_t>(20.0));
    EXPECT_EQ(locator.keys[1], to_fixed<x_t>(100.0));

    // tangent extender must run on final interval
    EXPECT_EQ(spline.segments[1].payload_id, spline.extend_final_tangent);
}

struct assembler_tangent_failure_test_t : Test
{
    using error_t = tangent_extender_t::error_t;
    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, segment_projector_t, locator_key_preparer_t,
        tangent_extender_t, 100>;

    workspace_t workspace{
        .completed_intervals = {
            {.subdomain = {.left_x = x_t{20}}, .segment = {.payload_id = 73}},
            {.subdomain = {.left_x = x_t{10}}, .segment = {.payload_id = 42}},
        },
    };
    typestate_t state{workspace};
    spline_t spline{
        .segments = {{{.payload_id = 1}, {.payload_id = 2}, {.payload_id = 3}}},
        .segment_locator = segment_locator_t{std::array{x_t{4}, x_t{5}}, x_t{6}, 2},
        .extend_final_tangent = 7,
    };
    spline_t const original_spline = spline;
    error_t const failure{
        .reason = spline_construction_error_reason_t::tangent_not_representable,
        .left = x_t{20},
        .right = x_t{100},
    };
};

TEST_F(assembler_tangent_failure_test_t, preserves_destination_and_exact_error)
{
    auto const sut = sut_t{
        .sort_intervals = {},
        .project_segments = {},
        .prepare_locator_keys = {},
        .extend_tangent = {.failure = &failure},
    };

    auto const result = sut(std::move(state), spline);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), failure);
    EXPECT_EQ(spline, original_spline);
}

//
// parameterized tests
//

struct spline_assembler_boundary_test_t : testing::TestWithParam<int_t>
{
    int_t count = GetParam();

    static constexpr auto domain_end_value = 100;
    x_t const x_max_fixed = to_fixed<x_t>(static_cast<float_t>(domain_end_value));
};

TEST_P(spline_assembler_boundary_test_t, handles_variable_segment_counts)
{
    auto workspace = workspace_t{};
    auto state = typestate_t{workspace};
    auto spline = spline_t{};

    // generate intervals as integers in descending order
    // set payload_id to the same value
    for (auto i = 0; i < count; ++i)
    {
        auto const descending_value = count - i;
        state.workspace.completed_intervals.push_back({
            .subdomain = {.left_x = x_t{descending_value * 10}},
            .segment = {.payload_id = descending_value},
        });
    }

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, segment_projector_t, locator_key_preparer_t,
        tangent_extender_t, domain_end_value>;
    auto sut = sut_t{};
    ASSERT_TRUE(sut(std::move(state), spline));

    EXPECT_TRUE(state.workspace.completed_intervals.empty());

    auto const& locator = spline.segment_locator;
    EXPECT_EQ(locator.count, count);
    EXPECT_EQ(locator.max_x, x_max_fixed);

    // segments must be sorted by ascending x, which is the same as payload_id
    for (auto i = 0; i < count; ++i)
    {
        auto const expected_id = i + 1;
        EXPECT_EQ(spline.segments[i].payload_id, expected_id);
    }

    // inner keys should be in sorted order up to count - 2
    for (auto i = 0; i < count - 1; ++i)
    {
        // key should match left bound of the *next* segment
        auto const expected_x = static_cast<float_t>(i + 2) * 10.0;
        EXPECT_EQ(locator.keys[i], to_fixed<x_t>(expected_x));
    }

    // outer keys are padding (from index count - 1 to end of array)
    for (auto i = count - 1; i < segment_locator_t::total_key_count; ++i) { EXPECT_EQ(locator.keys[i], x_max_fixed); }

    // tangent extender should run against final segment
    EXPECT_EQ(spline.segments[count - 1].payload_id, spline.extend_final_tangent);
}

INSTANTIATE_TEST_SUITE_P(
    spline_boundaries, spline_assembler_boundary_test_t, testing::Values(1, 2, segment_locator_t::max_segment_count));

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct spline_assembler_death_test_t : Test
{
    workspace_t workspace{};
    typestate_t state{workspace};
    spline_t spline{};

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, segment_projector_t, locator_key_preparer_t,
        tangent_extender_t, 100>;
    sut_t sut{};
};

TEST_F(spline_assembler_death_test_t, asserts_on_empty_workspace)
{
    EXPECT_DEATH(static_cast<void>(sut(std::move(state), spline)), "completed_intervals\\.empty");
}

TEST_F(spline_assembler_death_test_t, asserts_on_capacity_exceeded)
{
    // overfill the workspace by 1
    auto const overfill_count = segment_locator_t::max_segment_count + 1;
    for (auto i = 0; i < overfill_count; ++i) state.workspace.completed_intervals.push_back({});

    EXPECT_DEATH(static_cast<void>(sut(std::move(state), spline)), "max_segment_count");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace assembler_tests

} // namespace
} // namespace crv::spline
