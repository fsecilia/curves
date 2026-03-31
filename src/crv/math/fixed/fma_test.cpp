// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/fixed/fma.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using i32_0_t  = fixed_t<int32_t, 0>;
using i32_1_t  = fixed_t<int32_t, 1>;
using i32_15_t = fixed_t<int32_t, 15>;
using i32_30_t = fixed_t<int32_t, 30>;
using i64_30_t = fixed_t<int64_t, 30>;
using i64_31_t = fixed_t<int64_t, 31>;

constexpr auto rne   = rounding_modes::shr::nearest_even;
constexpr auto trunc = rounding_modes::shr::truncate;

// no bits are dropped during standard truncation/carry logic
static_assert(fma<i32_15_t>(i32_15_t{15}, i32_15_t{20}, i32_15_t{5}, rne) == i32_15_t{305});
static_assert(fma<i64_31_t, false>(i64_31_t{100}, i64_31_t{100}, i64_31_t{100}, rne) == i64_31_t{10100});

// saturation hooks before narrowing
static_assert(fma<i32_15_t, true>(i32_15_t{1000}, i32_15_t{1000}, i32_15_t{0}, rne)
              == literal<i32_15_t>(crv::max<int32_t>()));
static_assert(fma<true>(i32_15_t{-1000}, i32_15_t{1000}, i32_15_t{-50}, rne) == literal<i32_15_t>(crv::min<int32_t>()));
static_assert(fma<true>(i64_31_t{200000}, i64_31_t{200000}, i64_31_t{0}, rne)
              == literal<i64_31_t>(crv::max<int64_t>()));

// cascading precision
//
// This test shows how we represent values whose radix is outside the natural bounds of the underlying. It uses a 64-bit
// underlying to represent the top 64 bits of a 2^-80 value. In this scenario, the maximum value that can be
// represented at 80 bits of precision is just under 2^-17.
//
// a*b + c = (2^-18)*2^80*(2^20)*2^40*(2^20/2^120) + (19)*2^40*(2^20/2^40)
// = 2^62*2^60*2^-100 + (19)*2^40*2^-20
// = 2^122*2^-100 + 19*2^20
// = 2^122*2^-100 + 19*2^120*2^-100
// = (2^122 + 19*2^120)*2^-100
// = 2^22 + 19*2^20
using cascade_high_t = fixed_t<int64_t, 80>;
using cascade_mid_t  = fixed_t<int64_t, 40>;
using cascade_out_t  = fixed_t<int64_t, 20>;
static_assert(fma<cascade_out_t>(cascade_high_t::literal(1ll << 62), // (2^-18)*2^80
                                 cascade_mid_t{1ll << 20},           // (2^20)*2^40
                                 cascade_mid_t{19},                  // (19)*2^40
                                 rne)
              == cascade_out_t::literal((1 << 22) + (19 << 20)));

// exact cancellation
static_assert(fma<i32_15_t>(i32_15_t{50}, i32_15_t{-2}, i32_15_t{100}, trunc) == i32_15_t{0});

// 1.5 * 1.0 + 0 = 1.5; rne rounds up
static_assert(fma<fixed_t<int32_t, 0>>(i32_1_t::literal(3), i32_1_t::literal(2), i32_1_t::literal(0), rne)
              == fixed_t<int32_t, 0>{2});

// 2.5 * 1.0 + 0 = 2.5; rne rounds down
static_assert(fma<fixed_t<int32_t, 0>>(i32_1_t::literal(5), i32_1_t::literal(2), i32_1_t::literal(0), rne)
              == fixed_t<int32_t, 0>{2});

// mixed-width type resolution
using q4_t = fixed_t<int16_t, 4>;
static_assert(fma<i64_31_t>(q4_t{10}, i32_15_t{100}, i64_31_t{50000}, rne) == i64_31_t{51000});

// identity and zero preservation
static_assert(fma<i32_15_t>(i32_15_t{100}, i32_15_t{1}, i32_15_t{0}, trunc) == i32_15_t{100});
static_assert(fma<i64_31_t>(i64_31_t{9999}, i64_31_t{0}, i64_31_t{5555}, trunc) == i64_31_t{5555});

// negative saturation boundary
static_assert(fma<i32_15_t, true>(i32_15_t{-2000}, i32_15_t{2000}, i32_15_t{-500}, rne)
              == literal<i32_15_t>(crv::min<int32_t>()));

// extreme radix disparity; tests a tiny fractional product added to a value with massive fractional precision
using micro_frac_t = fixed_t<int64_t, 60>;
using macro_int_t  = fixed_t<int32_t, 2>;
static_assert(fma<micro_frac_t>(macro_int_t{2}, macro_int_t{3}, micro_frac_t{10}, trunc) == micro_frac_t{16});

// deduced-output wrapper overload
static_assert(fma<false>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i32_15_t{10});

// specified-output wrapper overload
static_assert(fma<i64_30_t, false>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i64_30_t{10});

// Base function resolution (resolves to the core fma, defaults saturate to false)
static_assert(fma<i32_15_t>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i32_15_t{10});

// --------------------------------------------------------------------------------------------------------------------
// Intermediate Radix Alignment Boundaries
// --------------------------------------------------------------------------------------------------------------------

// positive radix alignment boundary
// 5 * 5 = 25, + 1@q30 means (25 << 30) + (1 << 30) = 26 << 30
static_assert(fma<i32_0_t>(i32_0_t{5}, i32_0_t{5}, i32_30_t{1}, trunc).value == i32_0_t{26}.value);

// negative radix alignment boundary
// -5 * 5 = -25, + 1@q30 means -(25 << 30) + (1 << 30) = -24 << 30
static_assert(fma<i32_0_t>(i32_0_t{-5}, i32_0_t{5}, i32_30_t{1}, rne).value == i32_0_t{-24}.value);

} // namespace
} // namespace crv
