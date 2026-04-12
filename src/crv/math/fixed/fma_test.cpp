// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "fma.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

// common testing types
using i16_0_t  = fixed_t<int16_t, 0>;
using i16_4_t  = fixed_t<int16_t, 4>;
using i32_15_t = fixed_t<int32_t, 15>;
using i64_31_t = fixed_t<int64_t, 31>;

// common rounding modes
constexpr auto rne   = rounding_modes::shr::nearest_even;
constexpr auto trunc = rounding_modes::shr::truncate;

// autodeducing wrapper
template <typename out_t, bool saturate, typename multiplicand_t, typename multiplier_t, typename addend_t,
          typename rounding_mode_t>
constexpr auto fma(multiplicand_t multiplicand, multiplier_t multiplier, addend_t addend, rounding_mode_t rounding_mode)
    -> out_t
{
    return fma_t<out_t, multiplicand_t, multiplier_t, addend_t, saturate>{}(multiplicand, multiplier, addend,
                                                                            rounding_mode);
}

// --------------------------------------------------------------------------------------------------------------------
// Identity & Basic Operations
// --------------------------------------------------------------------------------------------------------------------

// multiplication identity and zero preservation
static_assert(fma<i32_15_t, true>(i32_15_t{100}, i32_15_t{1}, i32_15_t{0}, trunc) == i32_15_t{100});
static_assert(fma<i64_31_t, true>(i64_31_t{9999}, i64_31_t{0}, i64_31_t{5555}, trunc) == i64_31_t{5555});

// standard operation without truncation loss
static_assert(fma<i32_15_t, true>(i32_15_t{15}, i32_15_t{20}, i32_15_t{5}, rne) == i32_15_t{305});
static_assert(fma<i64_31_t, false>(i64_31_t{100}, i64_31_t{100}, i64_31_t{100}, rne) == i64_31_t{10100});

// exact cancellation (product perfectly offsets addend)
static_assert(fma<i32_15_t, true>(i32_15_t{50}, i32_15_t{-2}, i32_15_t{100}, trunc) == i32_15_t{0});

// --------------------------------------------------------------------------------------------------------------------
// Sign Permutations
// --------------------------------------------------------------------------------------------------------------------

// ensures shift extension and carry logic hold across all combinations of positive and negative terms
static_assert(fma<i32_15_t, false>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i32_15_t{10});    // +*+ + +
static_assert(fma<i32_15_t, false>(i32_15_t{2}, i32_15_t{3}, i32_15_t{-4}, trunc) == i32_15_t{2});    // +*+ + -
static_assert(fma<i32_15_t, false>(i32_15_t{2}, i32_15_t{-3}, i32_15_t{4}, trunc) == i32_15_t{-2});   // +*- + +
static_assert(fma<i32_15_t, false>(i32_15_t{2}, i32_15_t{-3}, i32_15_t{-4}, trunc) == i32_15_t{-10}); // +*- + -
static_assert(fma<i32_15_t, false>(i32_15_t{-2}, i32_15_t{3}, i32_15_t{4}, trunc) == i32_15_t{-2});   // -*+ + +
static_assert(fma<i32_15_t, false>(i32_15_t{-2}, i32_15_t{3}, i32_15_t{-4}, trunc) == i32_15_t{-10}); // -*+ + -
static_assert(fma<i32_15_t, false>(i32_15_t{-2}, i32_15_t{-3}, i32_15_t{4}, trunc) == i32_15_t{10});  // -*- + +
static_assert(fma<i32_15_t, false>(i32_15_t{-2}, i32_15_t{-3}, i32_15_t{-4}, trunc) == i32_15_t{2});  // -*- + -

// --------------------------------------------------------------------------------------------------------------------
// Radix Alignment & Mixed Type Resolution
// --------------------------------------------------------------------------------------------------------------------

// mixed-width type resolution (upcasting naturally)
static_assert(fma<i64_31_t, true>(i16_4_t{10}, i32_15_t{100}, i64_31_t{50000}, rne) == i64_31_t{51000});
static_assert(fma<i64_31_t, false>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i64_31_t{10});

// extreme radix disparity: adding a tiny fractional product to a macro value
using macro_int_t  = fixed_t<int32_t, 2>;
using micro_frac_t = fixed_t<int64_t, 60>;
static_assert(fma<micro_frac_t, true>(macro_int_t{2}, macro_int_t{3}, micro_frac_t{10}, trunc) == micro_frac_t{16});

// --------------------------------------------------------------------------------------------------------------------
// Rounding Modes
// --------------------------------------------------------------------------------------------------------------------

// fractional resolution tests using nearest even (rne)

// 2.4375 * 1.0 + 0.0 = 2.4375 -> rounds to 2
static_assert(fma<i16_0_t, false>(i16_4_t::literal(39), i16_4_t::literal(16), i16_4_t::literal(0), rne).value
              == i16_0_t{2}.value);

// 2.4375 * 1.0 + 0.0625 = 2.5 -> breaks tie to even (2)
static_assert(fma<i16_0_t, false>(i16_4_t::literal(39), i16_4_t::literal(16), i16_4_t::literal(1), rne).value
              == i16_0_t{2}.value);

// 2.4375 * 1.0 + 0.125 = 2.5625 -> rounds up to nearest (3)
static_assert(fma<i16_0_t, false>(i16_4_t::literal(39), i16_4_t::literal(16), i16_4_t::literal(2), rne).value
              == i16_0_t{3}.value);

// negative rounding (away from zero / toward zero based on tie-breaking)

// -2.5 -> -2
static_assert(fma<i16_0_t, false>(i16_4_t::literal(-39), i16_4_t::literal(16), i16_4_t::literal(-1), rne).value
              == i16_0_t{-2}.value);

// -3.5 -> -4
static_assert(fma<i16_0_t, false>(i16_4_t::literal(-55), i16_4_t::literal(16), i16_4_t::literal(-1), rne).value
              == i16_0_t{-4}.value);

// --------------------------------------------------------------------------------------------------------------------
// Saturation
// --------------------------------------------------------------------------------------------------------------------

// massive overshoots
static_assert(fma<i32_15_t, true>(i32_15_t{1000}, i32_15_t{1000}, i32_15_t{0}, rne)
              == literal<i32_15_t>(max<int32_t>()));
static_assert(fma<i32_15_t, true>(i32_15_t{-1000}, i32_15_t{1000}, i32_15_t{-50}, rne)
              == literal<i32_15_t>(min<int32_t>()));

// riding the exact boundary vs 1 ULP over
// exactly max: 1.0 * 1.0 + (max - 1.0) = max -> no saturation triggered
static_assert(fma<i16_4_t, true>(i16_4_t{1}, i16_4_t{1}, i16_4_t::literal(max<int16_t>() - (1 << 4)), trunc)
              == i16_4_t::literal(max<int16_t>()));
// max + 1 ULP: saturates to exactly max
static_assert(fma<i16_4_t, true>(i16_4_t{1}, i16_4_t{1}, i16_4_t::literal(max<int16_t>() - (1 << 4) + 1), trunc)
              == i16_4_t::literal(max<int16_t>()));

// exactly min: -1.0 * 1.0 + (min + 1.0) = min -> no saturation triggered
static_assert(fma<i16_4_t, true>(i16_4_t{-1}, i16_4_t{1}, i16_4_t::literal(min<int16_t>() + (1 << 4)), trunc)
              == i16_4_t::literal(min<int16_t>()));
// min - 1 ULP: saturates to exactly min
static_assert(fma<i16_4_t, true>(i16_4_t{-1}, i16_4_t{1}, i16_4_t::literal(min<int16_t>() + (1 << 4) - 1), trunc)
              == i16_4_t::literal(min<int16_t>()));

// --------------------------------------------------------------------------------------------------------------------
// Cascading Precision Bounds
// --------------------------------------------------------------------------------------------------------------------

// cascading precision
//
// This test shows how we represent values whose radix is outside the natural bounds of the underlying. It uses a 64-bit
// underlying to represent the top 64 bits of a 2^-80 value. In this scenario, the maximum value that can be
// represented at 80 bits of precision is just under 2^-17.
//
// a*b + c = (2^-18)*2^80*(2^20)*2^40*(2^20/2^120) + (19)*2^40*(2^20/2^40)
//         = 2^62*2^60*2^-100 + (19)*2^40*2^-20
//         = 2^122*2^-100 + 19*2^20
//         = 2^122*2^-100 + 19*2^120*2^-100
//         = (2^122 + 19*2^120)*2^-100
//         = 2^22 + 19*2^20
using cascade_high_t = fixed_t<int64_t, 80>;
using cascade_mid_t  = fixed_t<int64_t, 40>;
using cascade_out_t  = fixed_t<int64_t, 20>;
static_assert(fma<cascade_out_t, true>(cascade_high_t::literal(1ll << 62), // (2^-18)*2^80
                                       cascade_mid_t{1ll << 20},           // (2^20)*2^40
                                       cascade_mid_t{19},                  // (19)*2^40
                                       rne)
              == cascade_out_t::literal((1 << 22) + (19 << 20)));

} // namespace
} // namespace crv
