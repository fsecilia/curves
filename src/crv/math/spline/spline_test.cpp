// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

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

// -----------------------------------------------------------------------------------------
// minimum valid domain
// -----------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------
// death tests
// -----------------------------------------------------------------------------------------

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
