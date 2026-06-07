// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "assembler.hpp"
#include <crv/test/test.hpp>
#include <array>
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
        struct function_sample_t
        {
            float_t x{};

            constexpr auto operator==(function_sample_t const&) const noexcept -> bool = default;
        };
        function_sample_t left;

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
        {.subdomain = {.left{.x = 3.0}}, .segment = {.payload_id = 99}},
        {.subdomain = {.left{.x = 1.0}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left{.x = 2.0}}, .segment = {.payload_id = 73}},
    }};

    interval_sorter_t{}(actual);

    auto const expected = std::array<interval_t, 3>{{
        {.subdomain = {.left{.x = 1.0}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left{.x = 2.0}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left{.x = 3.0}}, .segment = {.payload_id = 99}},
    }};

    return expected == actual;
}
static_assert(test_interval_sorter());

constexpr auto test_key_padder() noexcept -> bool
{
    auto actual = std::array<x_t, 5>{x_t{1}, x_t{2}, x_t{0}, x_t{0}, x_t{0}};

    auto const pad_start = 2;
    auto const pad_value = x_t{99};

    key_padder_t{}(actual, pad_start, pad_value);

    auto const expected = std::array<x_t, 5>{x_t{1}, x_t{2}, pad_value, pad_value, pad_value};

    return expected == actual;
}
static_assert(test_key_padder());

constexpr auto test_key_padder_pad_all() noexcept -> bool
{
    auto actual = std::array<x_t, 3>{x_t{0}, x_t{0}, x_t{0}};

    key_padder_t{}(actual, 0, x_t{99});

    auto const expected = std::array<x_t, 3>{x_t{99}, x_t{99}, x_t{99}};

    return expected == actual;
}
static_assert(test_key_padder_pad_all());

TEST(spline_assembler_test, interval_unzipper_single_interval)
{
    auto const intervals
        = std::array<interval_t, 1>{{{.subdomain = {.left = {.x = 10.0}}, .segment = {.payload_id = 37}}}};

    auto const active_count = 1;
    auto actual_segments = std::array<segment_t, 1>{};
    auto actual_keys = std::array<x_t, 0>{};

    interval_unzipper_t{}(intervals, active_count, actual_segments, actual_keys);

    EXPECT_EQ(actual_segments[0], intervals[0].segment);
}

TEST(spline_assembler_test, interval_unzipper)
{
    auto const intervals = std::array<interval_t, 2>{{
        {.subdomain = {.left = {.x = 10.0}}, .segment = {.payload_id = 37}},
        {.subdomain = {.left = {.x = 20.0}}, .segment = {.payload_id = 73}},
    }};

    auto const active_count = 2;
    auto actual_segments = std::array<segment_t, 2>{};
    auto actual_keys = std::array<x_t, 1>{};

    interval_unzipper_t{}(intervals, active_count, actual_segments, actual_keys);

    auto const expected_segments = std::array{intervals[0].segment, intervals[1].segment};
    auto const expected_keys = std::array{to_fixed<x_t>(intervals[1].subdomain.left.x)};

    EXPECT_EQ(expected_segments, actual_segments);
    EXPECT_EQ(expected_keys, actual_keys);
}

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
};

struct spline_t
{
    using segment_locator_t = segment_locator_t;
    static constexpr auto max_segment_count = segment_locator_t::max_segment_count;

    struct payload_t
    {
        std::array<segment_t, max_segment_count> segments{};
        segment_locator_t segment_locator{};
        int_t extend_final_tangent{};
    };
    payload_t payload;
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
    constexpr auto operator()(interval_t const& interval) const noexcept -> int_t
    {
        return interval.segment.payload_id;
    }
};

TEST(spline_assembler_test, vs_real_dependencies)
{
    auto workspace = workspace_t{};
    auto state = typestate_t{workspace};

    // initialized out-of-order to prove the sorter runs in the pipeline
    state.workspace.completed_intervals = {
        {.subdomain = {.left = {.x = 20.0}}, .segment = {.payload_id = 73}},
        {.subdomain = {.left = {.x = 10.0}}, .segment = {.payload_id = 42}},
    };

    auto spline = spline_t{};

    constexpr auto domain_end_value = 100;

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, interval_unzipper_t, key_padder_t,
        tangent_extender_t, domain_end_value>;
    auto const sut = sut_t{};
    sut(std::move(state), spline);

    // workspace must be clear
    EXPECT_TRUE(state.workspace.completed_intervals.empty());

    // payload must be sorted
    EXPECT_EQ(spline.payload.segments[0].payload_id, 42);
    EXPECT_EQ(spline.payload.segments[1].payload_id, 73);

    // locator must be populated
    auto const& locator = spline.payload.segment_locator;
    EXPECT_EQ(locator.count, 2);
    EXPECT_EQ(locator.max_x, to_fixed<x_t>(100.0));

    // keys must contain left bounds of next segment, padded with max
    EXPECT_EQ(locator.keys[0], to_fixed<x_t>(20.0));
    EXPECT_EQ(locator.keys[1], to_fixed<x_t>(100.0));

    // tangent extender must run on final interval
    EXPECT_EQ(spline.payload.segments[1].payload_id, spline.payload.extend_final_tangent);
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
            .subdomain = {.left = {.x = static_cast<float_t>(descending_value) * 10.0}},
            .segment = {.payload_id = descending_value},
        });
    }

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, interval_unzipper_t, key_padder_t,
        tangent_extender_t, domain_end_value>;
    auto sut = sut_t{};
    sut(std::move(state), spline);

    EXPECT_TRUE(state.workspace.completed_intervals.empty());

    auto const& locator = spline.payload.segment_locator;
    EXPECT_EQ(locator.count, count);
    EXPECT_EQ(locator.max_x, x_max_fixed);

    // segments must be sorted by ascending x, which is the same as payload_id
    for (auto i = 0; i < count; ++i)
    {
        auto const expected_id = i + 1;
        EXPECT_EQ(spline.payload.segments[i].payload_id, expected_id);
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
    EXPECT_EQ(spline.payload.segments[count - 1].payload_id, spline.payload.extend_final_tangent);
}

INSTANTIATE_TEST_SUITE_P(
    spline_boundaries, spline_assembler_boundary_test_t, testing::Values(1, 2, segment_locator_t::max_segment_count));

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(spline_assembler_test, asserts_on_empty_workspace)
{
    auto workspace = workspace_t{};
    auto state = typestate_t{workspace};
    auto spline = spline_t{};

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, interval_unzipper_t, key_padder_t,
        tangent_extender_t, 100>;
    auto sut = sut_t{};

    EXPECT_DEATH(sut(std::move(state), spline), "completed_intervals\\.empty");
}

TEST(spline_assembler_test, asserts_on_capacity_exceeded)
{
    auto workspace = workspace_t{};
    auto state = typestate_t{workspace};
    auto spline = spline_t{};

    // overfill the workspace by 1
    auto const overfill_count = segment_locator_t::max_segment_count + 1;
    for (auto i = 0; i < overfill_count; ++i) state.workspace.completed_intervals.push_back({});

    using sut_t = assembler_t<typestate_t, interval_t, interval_sorter_t, interval_unzipper_t, key_padder_t,
        tangent_extender_t, 100>;
    auto sut = sut_t{};

    EXPECT_DEATH(sut(std::move(state), spline), "max_segment_count");
}

#endif

} // namespace assembler_tests

} // namespace
} // namespace crv::spline
