// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cubic_segment.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::fixed_point {
namespace {

using out_value_t = uint16_t;
using out_t       = fixed_t<out_value_t, 16>;

using dx_value_t = uint32_t;
using dx_t       = fixed_t<dx_value_t, 12>;
using wide_t     = widened_t<dx_value_t>;

using normalized_value_t = uint64_t;
using normalized_t       = fixed_t<normalized_value_t, 64>;

struct truncating_shifter_t
{
    template <typename value_t> [[nodiscard]] constexpr auto shift(value_t value, int_t count) const noexcept -> value_t
    {
        return count < 0 ? (value >> -count) : (value << count);
    }
};

template <typename t_out_t, typename t_in_t> struct monomial_t
{
    using out_t = t_out_t;
    using in_t  = t_in_t;

    [[nodiscard]] constexpr auto operator()(in_t t) const noexcept -> out_t { return out_t::convert(t); }
};

// segment is exactly 4.0 units wide; rwidth = 64 / 256 = 1 / 4.0 = 0.25
constexpr auto rwidth_frac_bits = 8;
constexpr auto rwidth           = dx_value_t{64};

// --------------------------------------------------------------------------------------------------------------------
// left shift tests
// --------------------------------------------------------------------------------------------------------------------

namespace left_shift_tests {

using monomial_t = monomial_t<out_t, normalized_t>;

using sut_t        = cubic_segment_t<monomial_t, dx_t, truncating_shifter_t>;
constexpr auto sut = sut_t{.monomial = monomial_t{}, .rwidth = rwidth, .rwidth_frac_bits = rwidth_frac_bits};

// min
static_assert(sut(dx_t::literal(0)).value == 0x0000);

// quarter
constexpr auto dx_quarter = dx_t::literal(1 << dx_t::frac_bits);
static_assert(sut(dx_quarter).value == 0x4000);

// mid
constexpr auto dx_mid = dx_t::literal(2 << dx_t::frac_bits);
static_assert(sut(dx_mid).value == 0x8000);

// max
constexpr auto dx_max = dx_t::literal((4 << dx_t::frac_bits) - 1);
static_assert(sut(dx_max).value == 0xFFFC);

} // namespace left_shift_tests

// --------------------------------------------------------------------------------------------------------------------
// right shift tests
// --------------------------------------------------------------------------------------------------------------------

namespace right_shift_tests {

using out_value_t = uint16_t;
using out_t       = fixed_t<out_value_t, 16>;

using dx_value_t = uint64_t;
using dx_t       = fixed_t<dx_value_t, 60>;

using monomial_t = monomial_t<out_t, normalized_t>;

using sut_t        = cubic_segment_t<monomial_t, dx_t, truncating_shifter_t>;
constexpr auto sut = sut_t{.monomial = {}, .rwidth = rwidth, .rwidth_frac_bits = rwidth_frac_bits};

// mid
constexpr auto dx_mid = dx_t::literal(2ULL << 60);
static_assert(sut(dx_mid).value == 0x8000);

} // namespace right_shift_tests

// --------------------------------------------------------------------------------------------------------------------
// shifter tests
// --------------------------------------------------------------------------------------------------------------------

namespace perturbing_shifter_tests {

// perturbs values detectibly to prove the shifter was used
struct perturbing_shifter_t
{
    // injects value of 53 into Q16, accounting for shift in monomial_t
    template <typename value_t> [[nodiscard]] constexpr auto shift(value_t value, int_t count) const noexcept -> value_t
    {
        auto const shifted = (count < 0) ? (value >> -count) : (value << count);

        return shifted + (53ULL << 48);
    }
};

using monomial_t = monomial_t<out_t, normalized_t>;

using sut_t        = cubic_segment_t<monomial_t, dx_t, perturbing_shifter_t>;
constexpr auto sut = sut_t{.monomial = monomial_t{}, .rwidth = rwidth, .rwidth_frac_bits = rwidth_frac_bits};

// min
static_assert(sut(dx_t::literal(0)).value == 53);

} // namespace perturbing_shifter_tests

// --------------------------------------------------------------------------------------------------------------------
// zero shift tests
// --------------------------------------------------------------------------------------------------------------------

namespace zero_shift_tests {

using out_value_t = uint16_t;
using out_t       = fixed_t<out_value_t, 16>;

using dx_value_t = uint64_t;
using dx_t       = fixed_t<dx_value_t, 56>; // 64 (normalized) - 56 (in) - 8 (rwidth) == 0

using monomial_t = monomial_t<out_t, normalized_t>;

using sut_t        = cubic_segment_t<monomial_t, dx_t, truncating_shifter_t>;
constexpr auto sut = sut_t{.monomial = {}, .rwidth = rwidth, .rwidth_frac_bits = rwidth_frac_bits};

constexpr auto dx_quarter = dx_t::literal(1ULL << dx_t::frac_bits);
static_assert(sut(dx_quarter).value == 0x4000);

} // namespace zero_shift_tests

} // namespace

// --------------------------------------------------------------------------------------------------------------------
// widening promotion tests
// --------------------------------------------------------------------------------------------------------------------

namespace widening_tests {

using out_value_t = uint64_t;
using out_t       = fixed_t<out_value_t, 44>;

using dx_value_t = uint32_t;
using dx_t       = fixed_t<dx_value_t, 16>;

using normalized_t = fixed_t<uint64_t, 44>;
using monomial_t   = monomial_t<out_t, normalized_t>;

using sut_t = cubic_segment_t<monomial_t, dx_t, truncating_shifter_t>;

// rwidth is 0.5 in Q28 (0x8000000)
// normalizing_shift = 44 (normalized) - 16 (in) - 28 (rwidth) = 0
constexpr auto rwidth = dx_value_t{0x8000000};
constexpr auto sut    = sut_t{.monomial = {}, .rwidth = rwidth, .rwidth_frac_bits = 28};

// rwidth is 0.5, dx max is 2.0, set dx to 1.5 in Q16 (1.5 * 0x10000 = 98304 = 0x00018000)
constexpr auto large_dx = dx_t::literal(0x18000);

// 0x18000*0x8000000 = 0xC000000000
static_assert(sut(large_dx).value == 0xC0000000000ULL);

} // namespace widening_tests

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS

namespace death_tests {

using monomial_t = monomial_t<out_t, normalized_t>;
using sut_t      = cubic_segment_t<monomial_t, dx_t, truncating_shifter_t>;

constexpr auto valid_dx = dx_t::literal(1 << dx_t::frac_bits);

TEST(spline_fixed_point_cubic_segment, violates_rwidth_greater_than_zero)
{
    constexpr auto bad_sut = sut_t{.monomial = {}, .rwidth = 0, .rwidth_frac_bits = rwidth_frac_bits};
    EXPECT_DEBUG_DEATH(static_cast<void>(bad_sut(valid_dx)), "rwidth > 0");
}

TEST(spline_fixed_point_cubic_segment, violates_rwidth_frac_bits_positive)
{
    constexpr auto bad_sut = sut_t{.monomial = {}, .rwidth = rwidth, .rwidth_frac_bits = -1};
    EXPECT_DEBUG_DEATH(static_cast<void>(bad_sut(valid_dx)), "rwidth_frac_bits >= 0");
}

TEST(spline_fixed_point_cubic_segment, violates_dx_lower_bound)
{
    using signed_dx_t  = fixed_t<int32_t, 12>;
    using signed_sut_t = cubic_segment_t<monomial_t, signed_dx_t, truncating_shifter_t>;

    constexpr auto sut         = signed_sut_t{.monomial = {}, .rwidth = rwidth, .rwidth_frac_bits = rwidth_frac_bits};
    constexpr auto negative_dx = signed_dx_t::literal(-1);

    EXPECT_DEBUG_DEATH(static_cast<void>(sut(negative_dx)), "0 <= dx.value");
}

TEST(spline_fixed_point_cubic_segment, violates_dx_upper_bound)
{
    constexpr auto sut = sut_t{.monomial = {}, .rwidth = rwidth, .rwidth_frac_bits = rwidth_frac_bits};

    // deliberately exceed 1/rwidth
    constexpr auto out_of_bounds_dx = dx_t::literal(5 << dx_t::frac_bits);

    EXPECT_DEBUG_DEATH(static_cast<void>(sut(out_of_bounds_dx)), "dx <");
}

} // namespace death_tests

#endif

} // namespace crv::spline::fixed_point
