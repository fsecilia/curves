// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using x_t = int8_t; // smaller than out
using y_t = int16_t; // wider than in
constexpr auto segment_count = 3;
constexpr auto x_max = x_t{5};

// adds dx to a distinct base_val; returns negative for extended tangents
struct segment_t
{
    using x_t = x_t;
    using y_t = y_t;

    y_t base_val;

    constexpr auto evaluate(x_t dx) const noexcept -> y_t { return static_cast<y_t>(base_val + dx); }
    constexpr auto extend_final_tangent(x_t dx) const noexcept -> y_t { return static_cast<y_t>(-(base_val + dx)); }

    constexpr auto is_valid() const noexcept -> bool { return true; }
};

// maps subdomains of width 2 to sequential indices
struct segment_locator_t
{
    struct result_t
    {
        int_t index;
        x_t origin;

        auto operator<=>(result_t const&) const noexcept -> auto = default;
        auto operator==(result_t const&) const noexcept -> bool = default;
    };

    constexpr auto locate(x_t x) const noexcept -> result_t
    {
        x_t const index = x / 2;
        return {.index = static_cast<int_t>(index), .origin = static_cast<x_t>(index * 2)};
    }

    x_t x_max_ = spline::x_max;
    int_t segment_count_ = spline::segment_count;
    constexpr auto x_max() const noexcept -> x_t { return x_max_; }
    constexpr auto segment_count() const noexcept -> int_t { return segment_count_; }

    constexpr auto is_valid() const noexcept -> bool { return true; }
    constexpr auto interval_width(int_t) const noexcept -> x_t { return x_t{0}; }
};

using sut_t = spline_t<segment_t, segment_locator_t>;

constexpr auto segments = std::array<segment_t, sut_t::max_segments>{{{10}, {20}, {30}}};
constexpr auto const sut = sut_t{segment_locator_t{x_max, segment_count}, segments};

// x in [0, 1] -> base 10
static_assert(sut(0) == 10);
static_assert(sut(1) == 11);

// x in [2, 3] -> base 20
static_assert(sut(2) == 20);
static_assert(sut(3) == 21);

// x in [4] -> base 30
static_assert(sut(4) == 30);

// maximum input value; x_max is 5, max_in is 127, dx = 122.
// segment 2 base_val is 30, result should be -(30 + 122) = -152.
static_assert(sut(max<x_t>()) == -152);

// extended final tangent: x >= x_max (5)
// routes to segment 2, passing dx = (x - x_max)
static_assert(sut(5) == -30); // -(30 + (5 - 5))
static_assert(sut(6) == -31); // -(30 + (6 - 5))

// ====================================================================================================================
// minimum valid domain
// ====================================================================================================================

namespace minimum_valid_domain {

// tests with single segment, checking that extend_final_tangent doesn't off-by-one when hitting segment_count_ - 1
using min_sut_t = spline_t<segment_t, segment_locator_t>;
constexpr auto const min_sut = min_sut_t{
    segment_locator_t{.x_max_ = 2, .segment_count_ = 1}, std::array<segment_t, min_sut_t::max_segments>{{{10}}}};

static_assert(min_sut(0) == 10);
static_assert(min_sut(1) == 11);

// extended tangent delegates to segment 0
static_assert(min_sut(2) == -10); // -(10 + (2 - 2))
static_assert(min_sut(3) == -11); // -(10 + (3 - 2))

} // namespace minimum_valid_domain

// ====================================================================================================================
// is_valid
// ====================================================================================================================

namespace is_valid_tests {

struct segment_t
{
    using x_t = x_t;
    using y_t = y_t;

    bool valid{true};
    x_t width_{4};

    constexpr auto evaluate(x_t) const noexcept -> y_t { return 0; }
    constexpr auto extend_final_tangent(x_t) const noexcept -> y_t { return 0; }
    constexpr auto is_valid() const noexcept -> bool { return valid; }
    constexpr auto width() const noexcept -> x_t { return width_; }
};
using segments_t = std::array<segment_t, sut_t::max_segments>;

struct locator_t
{
    struct result_t
    {
        int_t index;
        x_t origin;
    };

    constexpr auto locate(x_t) const noexcept -> result_t { return {0, 0}; }
    constexpr auto x_max() const noexcept -> x_t { return spline::x_max; }

    bool valid{true};
    x_t interval_width_{4};
    int_t segment_count_{2};
    constexpr auto is_valid() const noexcept -> bool { return valid; }
    constexpr auto interval_width(int_t) const noexcept -> x_t { return interval_width_; }
    constexpr auto segment_count() const noexcept -> int_t { return segment_count_; }
};

using sut_t = spline_t<segment_t, locator_t>;

constexpr auto make_segments(bool s0_valid, bool s1_valid) -> segments_t
{
    auto segments = segments_t{};
    segments[0].valid = s0_valid;
    segments[1].valid = s1_valid;
    return segments;
}

// all valid
static_assert(sut_t{locator_t{}, make_segments(true, true), 0}.is_valid());

// invalid prev_segment_index < 0
static_assert(!sut_t{locator_t{}, make_segments(true, true), -1}.is_valid());

// invalid prev_segment_index >= segment_count
static_assert(!sut_t{locator_t{}, make_segments(true, true), 2}.is_valid());

// locator reports zero segments: no valid prev_segment_index exists
static_assert(!sut_t{locator_t{.segment_count_ = 0}, make_segments(true, true), 0}.is_valid());

// invalid locator
static_assert(!sut_t{locator_t{.valid = false}, make_segments(true, true), 0}.is_valid());

// invalid first segment
static_assert(!sut_t{locator_t{}, make_segments(false, true), 0}.is_valid());

// invalid second segment
static_assert(!sut_t{locator_t{}, make_segments(true, false), 0}.is_valid());

// cross-check: interval_width must equal segment width exactly (construction produces power-of-2 widths)
constexpr auto make_segments_with_width(x_t width0, x_t width1) -> segments_t
{
    auto segments = segments_t{};
    segments[0].width_ = width0;
    segments[1].width_ = width1;
    return segments;
}

// first segment narrower than interval
static_assert(!sut_t{locator_t{.interval_width_ = 4}, make_segments_with_width(2, 4), 0}.is_valid());

// second segment narrower than interval
static_assert(!sut_t{locator_t{.interval_width_ = 4}, make_segments_with_width(4, 2), 0}.is_valid());

// first segment wider than interval (rejected only under strict equality)
static_assert(!sut_t{locator_t{.interval_width_ = 4}, make_segments_with_width(8, 4), 0}.is_valid());

// second segment wider than interval
static_assert(!sut_t{locator_t{.interval_width_ = 4}, make_segments_with_width(4, 8), 0}.is_valid());

} // namespace is_valid_tests

// ====================================================================================================================
// prefetch
// ====================================================================================================================

struct spline_prefetch_test_t : Test
{
    struct segment_t
    {
        using x_t = x_t;
        using y_t = y_t;

        alignas(32) std::array<std::byte, 32> padding;

        constexpr auto evaluate(x_t) const noexcept -> y_t { return y_t{0}; }
        constexpr auto extend_final_tangent(x_t) const noexcept -> y_t { return y_t{0}; }
        constexpr auto is_valid() const noexcept -> bool { return true; }
    };

    struct mock_prefetcher_t
    {
        MOCK_METHOD(void, prefetch, (void const* address), (const, noexcept));

        virtual ~mock_prefetcher_t() = default;
    };
    StrictMock<mock_prefetcher_t> mock_prefetcher;

    struct prefetcher_t
    {
        mock_prefetcher_t* mock = nullptr;

        auto prefetch(void const* address) const noexcept -> void { mock->prefetch(address); }
    };
    prefetcher_t prefetcher{&mock_prefetcher};

    struct mock_locator_t
    {
        using result_t = segment_locator_t::result_t;

        virtual ~mock_locator_t() = default;

        MOCK_METHOD(result_t, locate, (x_t), (const, noexcept));
        MOCK_METHOD(void, prefetch, (mock_prefetcher_t const& prefetcher), (const, noexcept));
    };
    StrictMock<mock_locator_t> mock_locator;

    struct locator_t
    {
        using result_t = mock_locator_t::result_t;

        mock_locator_t* mock = nullptr;

        auto segment_count() const noexcept -> int_t { return spline::segment_count; }
        auto x_max() const noexcept -> x_t { return spline::x_max; }
        auto is_valid() const noexcept -> bool { return true; }

        auto locate(x_t in) const noexcept -> result_t { return mock->locate(in); }
        auto prefetch(auto const& prefetcher) const noexcept -> void { mock->prefetch(*prefetcher.mock); }
    };

    using sut_t = spline_t<segment_t, locator_t>;
    sut_t sut{locator_t{.mock = &mock_locator}, std::array<segment_t, sut_t::max_segments>{}};

    static constexpr auto expected_segment = segment_count - 1;
    static constexpr auto expected_fetch_distance = 2 * sizeof(segment_t);
};

TEST_F(spline_prefetch_test_t, initial_state_prefetches_adjacents_at_head)
{
    EXPECT_CALL(mock_locator, prefetch(Ref(mock_prefetcher)));

    void const* prefetched_cache_lines[2];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[1]));
    }

    sut.prefetch(prefetcher);

    auto const actual_distance = static_cast<std::byte const*>(prefetched_cache_lines[1])
        - static_cast<std::byte const*>(prefetched_cache_lines[0]);

    EXPECT_EQ(expected_fetch_distance, actual_distance);
}

TEST_F(spline_prefetch_test_t, mutates_index_and_prefetches_new_adjacents)
{
    // prefetch initial to find location of segments
    EXPECT_CALL(mock_locator, prefetch(Ref(mock_prefetcher)));
    void const* initial_prefetched_cache_lines[2];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&initial_prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&initial_prefetched_cache_lines[1]));
    }
    sut.prefetch(prefetcher);
    auto const* segments = static_cast<std::byte const*>(initial_prefetched_cache_lines[0]) + sizeof(segment_t);

    // call into sut to get it to cache a new previous segment; previous_cache_line_ should become 2
    auto const x = x_t{expected_segment};
    EXPECT_CALL(mock_locator, locate(x))
        .WillOnce(Return(segment_locator_t::result_t{.index = expected_segment, .origin = 0}));
    sut(x);

    // prefetch again to see that previous_cache_line_ moved correctly
    EXPECT_CALL(mock_locator, prefetch(Ref(mock_prefetcher)));

    void const* prefetched_cache_lines[2];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[1]));
    }

    sut.prefetch(prefetcher);

    auto const actual_distance = static_cast<std::byte const*>(prefetched_cache_lines[1])
        - static_cast<std::byte const*>(prefetched_cache_lines[0]);
    EXPECT_EQ(expected_fetch_distance, actual_distance);

    auto const expected_cache_line_0 = segments + (expected_segment - 1) * sizeof(segment_t);
    EXPECT_EQ(expected_cache_line_0, static_cast<std::byte const*>(prefetched_cache_lines[0]));
}

// ====================================================================================================================
// death tests
// ====================================================================================================================

#if defined CRV_ENABLE_DEATH_TESTS

struct spline_death_test_t : Test
{
    static constexpr auto segments = std::array<segment_t, sut_t::max_segments>{};
};

TEST_F(spline_death_test_t, establish_valid_case)
{
    static_cast<void>(sut_t{segment_locator_t{}, segments});
}

TEST_F(spline_death_test_t, call_operator_catches_negative_x)
{
    EXPECT_DEATH((sut_t{segment_locator_t{}, segments}(x_t{-1})), "input out of bounds");
}

// --------------------------------------------------------------------------------------------------------------------
// call operator interactions with segment_locator_t
// --------------------------------------------------------------------------------------------------------------------

struct spline_death_test_call_operator_malicious_locator_t : spline_death_test_t
{
    struct malicious_locator_t
    {
        using result_t = segment_locator_t::result_t;

        x_t index = 0;
        x_t origin = 0;
        constexpr auto locate(x_t) const noexcept -> result_t { return {.index = index, .origin = origin}; }

        constexpr auto x_max() const noexcept -> x_t { return spline::x_max; }
        constexpr auto segment_count() const noexcept -> int_t { return spline::segment_count; }
    };

    using sut_t = spline_t<segment_t, malicious_locator_t>;
};

TEST_F(spline_death_test_call_operator_malicious_locator_t, negative_index)
{
    auto const sut = sut_t{malicious_locator_t{.index = -1}, segments};

    EXPECT_DEATH(sut(0), "index out of bounds");
}

TEST_F(spline_death_test_call_operator_malicious_locator_t, oor_index)
{
    auto const sut = sut_t{malicious_locator_t{.index = 127}, segments};

    EXPECT_DEATH(sut(0), "index out of bounds");
}

TEST_F(spline_death_test_call_operator_malicious_locator_t, negative_origin)
{
    auto const sut = sut_t{malicious_locator_t{.origin = -1}, segments};

    EXPECT_DEATH(sut(0), "origin out of range");
}

TEST_F(spline_death_test_call_operator_malicious_locator_t, oor_origin)
{
    auto const x = x_t{x_max - 2};
    auto const sut = sut_t{malicious_locator_t{.origin = static_cast<x_t>(x + 1)}, segments};

    EXPECT_DEATH(sut(x), "origin out of range");
}

#endif

} // namespace
} // namespace crv::spline
