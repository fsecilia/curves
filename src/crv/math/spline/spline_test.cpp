// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <new>

namespace crv::spline {
namespace {

using in_t = uint8_t; // smaller than out, unsigned
using out_t = int16_t; // wider than in, signed
constexpr auto segment_count = 3;
constexpr auto x_max = 5;

// adds dx to a distinct base_val; returns negative for extended tangents
struct segment_t
{
    using in_t = in_t;
    using out_t = out_t;

    out_t base_val;

    constexpr auto operator()(in_t dx) const noexcept -> out_t { return static_cast<out_t>(base_val + dx); }
    constexpr auto extend_final_tangent(in_t dx) const noexcept -> out_t
    {
        return static_cast<out_t>(-(base_val + dx));
    }

    constexpr auto is_valid() const noexcept -> bool { return true; }
};

// maps subdomains of width 2 to sequential indices
struct segment_locator_t
{
    struct result_t
    {
        int_t index;
        in_t origin;

        auto operator<=>(result_t const&) const noexcept -> auto = default;
        auto operator==(result_t const&) const noexcept -> bool = default;
    };

    constexpr auto operator()(in_t x) const noexcept -> result_t
    {
        in_t const index = x / 2;
        return {.index = static_cast<int_t>(index), .origin = static_cast<in_t>(index * 2)};
    }

    constexpr auto is_valid(int_t) const noexcept -> bool { return true; }
};

using sut_t = spline_t<segment_t, segment_locator_t>;

// initialize 3 segments needed for the domain up to x_max
constexpr auto const sut
    = sut_t{segment_locator_t{}, std::array<segment_t, sut_t::max_segments>{{{10}, {20}, {30}}}, segment_count, x_max};

// x in [0, 1] -> base 10
static_assert(sut(0) == 10);
static_assert(sut(1) == 11);

// x in [2, 3] -> base 20
static_assert(sut(2) == 20);
static_assert(sut(3) == 21);

// x in [4] -> base 30
static_assert(sut(4) == 30);

// maximum input value; x_max is 5, max_in is 255, dx = 250.
// segment 2 base_val is 30, result should be -(30 + 250) = -280.
static_assert(sut(max<in_t>()) == -280);

// extended final tangent: x >= x_max (5)
// routes to segment 2, passing dx = (x - x_max)
static_assert(sut(5) == -30); // -(30 + (5 - 5))
static_assert(sut(6) == -31); // -(30 + (6 - 5))

// --------------------------------------------------------------------------------------------------------------------
// minimum valid domain
// --------------------------------------------------------------------------------------------------------------------

namespace minimum_valid_domain {

// tests with single segment, checking that extend_final_tangent doesn't off-by-one when hitting segment_count_ - 1
using min_sut_t = spline_t<segment_t, segment_locator_t>;
constexpr auto const min_sut
    = min_sut_t{segment_locator_t{}, std::array<segment_t, min_sut_t::max_segments>{{{10}}}, 1, 2};

static_assert(min_sut(0) == 10);
static_assert(min_sut(1) == 11);

// extended tangent delegates to segment 0
static_assert(min_sut(2) == -10); // -(10 + (2 - 2))
static_assert(min_sut(3) == -11); // -(10 + (3 - 2))

} // namespace minimum_valid_domain

// --------------------------------------------------------------------------------------------------------------------
// is_valid
// --------------------------------------------------------------------------------------------------------------------

namespace is_valid_tests {

struct segment_t
{
    using in_t = in_t;
    using out_t = out_t;

    bool valid{true};

    constexpr auto operator()(in_t) const noexcept -> out_t { return 0; }
    constexpr auto extend_final_tangent(in_t) const noexcept -> out_t { return 0; }
    constexpr auto is_valid() const noexcept -> bool { return valid; }
};
using segments_t = std::array<segment_t, sut_t::max_segments>;

struct locator_t
{
    struct result_t
    {
        int_t index;
        in_t origin;
    };

    bool valid{true};

    constexpr auto operator()(in_t) const noexcept -> result_t { return {0, 0}; }
    constexpr auto is_valid(int_t) const noexcept -> bool { return valid; }
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
static_assert(sut_t{locator_t{.valid = true}, make_segments(true, true), 2, 5}.is_valid());

// invalid domain, x_max <= 0
static_assert(!sut_t{locator_t{.valid = true}, make_segments(true, true), 2, 0}.is_valid());

// invalid locator
static_assert(!sut_t{locator_t{.valid = false}, make_segments(true, true), 2, 5}.is_valid());

// invalid first segment
static_assert(!sut_t{locator_t{.valid = true}, make_segments(false, true), 2, 5}.is_valid());

// invalid second segment
static_assert(!sut_t{locator_t{.valid = true}, make_segments(true, false), 2, 5}.is_valid());

} // namespace is_valid_tests

// --------------------------------------------------------------------------------------------------------------------
// prefetch
// --------------------------------------------------------------------------------------------------------------------

struct spline_prefetch_test_t : Test
{
    struct segment_t
    {
        using in_t = in_t;
        using out_t = out_t;

        alignas(32) std::array<std::byte, 32> padding;

        constexpr auto operator()(in_t) const noexcept -> out_t { return out_t{0}; }
        constexpr auto extend_final_tangent(in_t) const noexcept -> out_t { return out_t{0}; }
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

        MOCK_METHOD(result_t, call, (in_t), (const, noexcept));
        MOCK_METHOD(bool, is_valid, (int_t), (const, noexcept));
        MOCK_METHOD(void, prefetch, (mock_prefetcher_t const& prefetcher), (const, noexcept));
    };
    StrictMock<mock_locator_t> mock_locator;

    struct locator_t
    {
        using result_t = mock_locator_t::result_t;

        mock_locator_t* mock = nullptr;

        auto operator()(in_t in) const noexcept -> result_t { return mock->call(in); };
        auto is_valid(int_t segment_count) const noexcept -> bool { return mock->is_valid(segment_count); }
        auto prefetch(auto const& prefetcher) const noexcept -> void { mock->prefetch(*prefetcher.mock); }
    };

    using sut_t = spline_t<segment_t, locator_t>;
    sut_t sut{locator_t{&mock_locator}, std::array<segment_t, sut_t::max_segments>{}, 5, x_max};

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
    auto const expected_segment = 2;
    auto const x = in_t{expected_segment};
    EXPECT_CALL(mock_locator, call(x))
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

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS

namespace death_tests {

TEST(spline_death_test, catches_zero_segment_count)
{
    EXPECT_DEATH(
        (sut_t{segment_locator_t{}, std::array<segment_t, sut_t::max_segments>{}, 0, x_max}), "nonzero segment count");
}

TEST(spline_death_test, catches_max_segments_exceeded)
{
    EXPECT_DEATH(
        (sut_t{segment_locator_t{}, std::array<segment_t, sut_t::max_segments>{}, sut_t::max_segments + 1, x_max}),
        "segment count exceeds capacity");
}

TEST(spline_death_test, index_out_of_bounds_locator)
{
    // malicious locator returning an oob index
    struct index_out_of_bounds_locator_t
    {
        using result_t = segment_locator_t::result_t;

        constexpr auto operator()(in_t) const noexcept -> result_t { return {.index = 999, .origin = 0}; }
    };

    using oob_sut_t = spline_t<segment_t, index_out_of_bounds_locator_t>;
    auto const sut = oob_sut_t{index_out_of_bounds_locator_t{},
        std::array<segment_t, oob_sut_t::max_segments>{{{10}, {20}, {30}}}, segment_count, x_max};

    EXPECT_DEATH(sut(0), "index out of bounds");
}

TEST(spline_death_test, origin_underflow_locator)
{
    // malicious locator returning origin > x
    struct origin_underflow_locator_t
    {
        using result_t = segment_locator_t::result_t;

        constexpr auto operator()(in_t x) const noexcept -> result_t
        {
            return {.index = 0, .origin = static_cast<in_t>(x + 1)};
        }
    };

    using oob_sut_t = spline_t<segment_t, origin_underflow_locator_t>;
    auto const sut
        = oob_sut_t{origin_underflow_locator_t{}, std::array<segment_t, oob_sut_t::max_segments>{{{10}}}, 1, x_max};

    EXPECT_DEATH(sut(0), "origin underflows");
}

} // namespace death_tests

#endif

} // namespace
} // namespace crv::spline
