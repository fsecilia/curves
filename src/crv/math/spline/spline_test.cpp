// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using x_t = int8_t; // narrower than out
using y_t = int16_t; // wider than in
constexpr auto segment_count = 3;
constexpr auto x_max = x_t{5};

// adds dx to a distinct base_val
struct segment_t
{
    using x_t = x_t;
    using y_t = y_t;

    y_t base_val;

    constexpr auto operator()(x_t x) const noexcept -> y_t { return static_cast<y_t>(base_val + x); }
};

// subtracts dx from a distinct base_val
struct extended_tangent_t
{
    y_t base_val;

    constexpr auto operator()(x_t x) const noexcept -> y_t { return static_cast<y_t>(base_val - x); }
};

// maps subdomains of width 2 to sequential indices
struct segment_locator_t
{
    static constexpr auto max_segment_count = segment_count;

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
    constexpr auto x_max() const noexcept -> x_t { return x_max_; }

    int_t segment_count_ = spline::segment_count;
    constexpr auto segment_count() const noexcept -> int_t { return segment_count_; }

    constexpr auto is_valid() const noexcept -> bool { return true; }
};

using sut_t = spline_t<segment_t, extended_tangent_t, segment_locator_t>;

constexpr auto segments = std::array<segment_t, sut_t::max_segment_count>{{{10}, {20}, {30}}};
constexpr auto extended_tangent = extended_tangent_t{-40};
constexpr auto const sut = sut_t{segment_locator_t{x_max, segment_count}, segments, extended_tangent};

// x in [0, 1] -> base 10
static_assert(sut(0) == 10);
static_assert(sut(1) == 11);

// x in [2, 3] -> base 20
static_assert(sut(2) == 20);
static_assert(sut(3) == 21);

// x in [4] -> base 30
static_assert(sut(4) == 30);

// extended final tangent: x >= x_max (5)
static_assert(sut(5) == -40); // -40 - (5 - 5)
static_assert(sut(6) == -41); // -40 - (5 - 6)

// maximum input value; x_max is 5, max_in is 127, x = 122.
// extended base_val is -40, result should be -40 - 122 = -162.
static_assert(sut(max<x_t>()) == -162);

// ====================================================================================================================
// prefetch
// ====================================================================================================================

// TODO: figure out how to test prefetching the tangent extension segment

struct spline_prefetch_test_t : Test
{
    struct segment_t
    {
        using x_t = x_t;
        using y_t = y_t;

        alignas(32) std::array<std::byte, 32> padding;

        constexpr auto operator()(x_t) const noexcept -> y_t { return y_t{0}; }
    };

    struct extended_tangent_t
    {
        constexpr auto operator()(x_t) const noexcept -> y_t { return y_t{0}; }
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
        static constexpr auto max_segment_count = segment_count;

        using result_t = mock_locator_t::result_t;

        mock_locator_t* mock = nullptr;

        auto segment_count() const noexcept -> int_t { return spline::segment_count; }
        auto x_max() const noexcept -> x_t { return spline::x_max; }
        auto is_valid() const noexcept -> bool { return true; }

        auto locate(x_t in) const noexcept -> result_t { return mock->locate(in); }
        auto prefetch(auto const& prefetcher) const noexcept -> void { mock->prefetch(*prefetcher.mock); }
    };

    using sut_t = spline_t<segment_t, extended_tangent_t, locator_t>;
    sut_t sut{locator_t{.mock = &mock_locator}, {}, {}};

    static constexpr auto expected_segment = segment_count - 1;
    static constexpr auto expected_fetch_distance = 2 * sizeof(segment_t);
};

TEST_F(spline_prefetch_test_t, initial_state_prefetches_adjacents_at_head)
{
    EXPECT_CALL(mock_locator, prefetch(Ref(mock_prefetcher)));

    void const* prefetched_cache_lines[3];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[1]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[2]));
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
    void const* initial_prefetched_cache_lines[3];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&initial_prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&initial_prefetched_cache_lines[1]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&initial_prefetched_cache_lines[2]));
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

    void const* prefetched_cache_lines[3];
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[0]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[1]));
        EXPECT_CALL(mock_prefetcher, prefetch(_)).WillOnce(SaveArg<0>(&prefetched_cache_lines[2]));
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
    static constexpr auto segments = std::array<segment_t, sut_t::max_segment_count>{};
    static constexpr auto extended_tangent = extended_tangent_t{};
};

TEST_F(spline_death_test_t, establish_valid_case)
{
    static_cast<void>(sut_t{segment_locator_t{}, segments, extended_tangent});
}

TEST_F(spline_death_test_t, call_operator_catches_negative_x)
{
    EXPECT_DEATH((sut_t{segment_locator_t{}, segments, extended_tangent}(x_t{-1})), "input out of bounds");
}

// --------------------------------------------------------------------------------------------------------------------
// call operator interactions with segment_locator_t
// --------------------------------------------------------------------------------------------------------------------

struct spline_death_test_call_operator_malicious_locator_t : spline_death_test_t
{
    struct malicious_locator_t
    {
        static constexpr auto max_segment_count = segment_count;

        using result_t = segment_locator_t::result_t;

        x_t index = 0;
        x_t origin = 0;
        constexpr auto locate(x_t) const noexcept -> result_t { return {.index = index, .origin = origin}; }

        constexpr auto x_max() const noexcept -> x_t { return spline::x_max; }
        constexpr auto segment_count() const noexcept -> int_t { return spline::segment_count; }
    };

    using sut_t = spline_t<segment_t, extended_tangent_t, malicious_locator_t>;
};

TEST_F(spline_death_test_call_operator_malicious_locator_t, negative_index)
{
    auto const sut = sut_t{malicious_locator_t{.index = -1}, segments, extended_tangent};

    EXPECT_DEATH(sut(0), "index out of bounds");
}

TEST_F(spline_death_test_call_operator_malicious_locator_t, oor_index)
{
    auto const sut = sut_t{malicious_locator_t{.index = 127}, segments, extended_tangent};

    EXPECT_DEATH(sut(0), "index out of bounds");
}

TEST_F(spline_death_test_call_operator_malicious_locator_t, negative_origin)
{
    auto const sut = sut_t{malicious_locator_t{.origin = -1}, segments, extended_tangent};

    EXPECT_DEATH(sut(0), "origin out of range");
}

TEST_F(spline_death_test_call_operator_malicious_locator_t, oor_origin)
{
    auto const x = x_t{x_max - 2};
    auto const sut = sut_t{malicious_locator_t{.origin = static_cast<x_t>(x + 1)}, segments, extended_tangent};

    EXPECT_DEATH(sut(x), "origin out of range");
}

#endif

} // namespace
} // namespace crv::spline
