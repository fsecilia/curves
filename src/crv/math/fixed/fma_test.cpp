// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "fma.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

// common testing types
using i16_0_t = fixed_t<int16_t, 0>;
using u16_0_t = fixed_t<uint16_t, 0>;
using i16_4_t = fixed_t<int16_t, 4>;
using u16_4_t = fixed_t<uint16_t, 4>;
using i32_0_t = fixed_t<int32_t, 0>;
using i32_15_t = fixed_t<int32_t, 15>;
using u32_15_t = fixed_t<uint32_t, 15>;
using i64_31_t = fixed_t<int64_t, 31>;

using pure_int_t = fixed_t<int16_t, 0>;
using frac_t = fixed_t<int16_t, 12>;

// common rounding modes
constexpr auto rne = shifter_t{rounding_modes::shr::nearest_even};
constexpr auto trunc = shifter_t{rounding_modes::shr::truncate};

constexpr auto saturate = overflow_policy_t::saturate;
constexpr auto wrap = overflow_policy_t::wrap;

// autodeducing wrapper
template <typename out_t, overflow_policy_t saturation, typename multiplicand_t, typename multiplier_t,
    typename addend_t, typename shifter_t>
constexpr auto fma(multiplicand_t multiplicand, multiplier_t multiplier, addend_t addend, shifter_t shifter) -> out_t
{
    return fma_t<out_t, multiplicand_t, multiplier_t, addend_t, shifter_t, saturation>{std::move(shifter)}(
        multiplicand, multiplier, addend);
}

// --------------------------------------------------------------------------------------------------------------------
// Identity & Basic Operations
// --------------------------------------------------------------------------------------------------------------------

// multiplication identity and zero preservation
static_assert(fma<i32_15_t, saturate>(i32_15_t{100}, i32_15_t{1}, i32_15_t{0}, trunc) == i32_15_t{100});
static_assert(fma<i64_31_t, saturate>(i64_31_t{9999}, i64_31_t{0}, i64_31_t{5555}, trunc) == i64_31_t{5555});

// standard operation without truncation loss
static_assert(fma<i32_15_t, saturate>(i32_15_t{15}, i32_15_t{20}, i32_15_t{5}, rne) == i32_15_t{305});
static_assert(fma<i64_31_t, wrap>(i64_31_t{100}, i64_31_t{100}, i64_31_t{100}, rne) == i64_31_t{10100});

// exact cancellation; product perfectly offsets addend
static_assert(fma<i32_15_t, saturate>(i32_15_t{50}, i32_15_t{-2}, i32_15_t{100}, trunc) == i32_15_t{0});

// --------------------------------------------------------------------------------------------------------------------
// Sign Permutations
// --------------------------------------------------------------------------------------------------------------------

// ensures shift extension and carry logic hold across all combinations of positive and negative terms
static_assert(fma<i32_15_t, wrap>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i32_15_t{10}); // +*+ + +
static_assert(fma<i32_15_t, wrap>(i32_15_t{2}, i32_15_t{3}, i32_15_t{-4}, trunc) == i32_15_t{2}); // +*+ + -
static_assert(fma<i32_15_t, wrap>(i32_15_t{2}, i32_15_t{-3}, i32_15_t{4}, trunc) == i32_15_t{-2}); // +*- + +
static_assert(fma<i32_15_t, wrap>(i32_15_t{2}, i32_15_t{-3}, i32_15_t{-4}, trunc) == i32_15_t{-10}); // +*- + -
static_assert(fma<i32_15_t, wrap>(i32_15_t{-2}, i32_15_t{3}, i32_15_t{4}, trunc) == i32_15_t{-2}); // -*+ + +
static_assert(fma<i32_15_t, wrap>(i32_15_t{-2}, i32_15_t{3}, i32_15_t{-4}, trunc) == i32_15_t{-10}); // -*+ + -
static_assert(fma<i32_15_t, wrap>(i32_15_t{-2}, i32_15_t{-3}, i32_15_t{4}, trunc) == i32_15_t{10}); // -*- + +
static_assert(fma<i32_15_t, wrap>(i32_15_t{-2}, i32_15_t{-3}, i32_15_t{-4}, trunc) == i32_15_t{2}); // -*- + -

// --------------------------------------------------------------------------------------------------------------------
// Radix Alignment & Mixed Type Resolution
// --------------------------------------------------------------------------------------------------------------------

// mixed-width type resolution; natural upcasts
static_assert(fma<i64_31_t, saturate>(i16_4_t{10}, i32_15_t{100}, i64_31_t{50000}, rne) == i64_31_t{51000});
static_assert(fma<i64_31_t, wrap>(i32_15_t{2}, i32_15_t{3}, i32_15_t{4}, trunc) == i64_31_t{10});

// exact cancellation via wrapping: micro_frac_t{10} wraps to -6.0 (Q60 range is ~[-8,8))
// 2*3 = 6.0 exactly offsets the wrapped addend; micro_frac_t{16} wraps to 0.0
using macro_int_t = fixed_t<int32_t, 2>;
using micro_frac_t = fixed_t<int64_t, 60>;
static_assert(fma<micro_frac_t, saturate>(macro_int_t{2}, macro_int_t{3}, micro_frac_t{10}, trunc) == micro_frac_t{0});

// --------------------------------------------------------------------------------------------------------------------
// Rounding (RNE)
// --------------------------------------------------------------------------------------------------------------------

// 2.4375 * 1.0 + 0.0 = 2.4375 -> 2
static_assert(fma<i16_0_t, wrap>(i16_4_t::literal(39), i16_4_t{1}, i16_4_t::literal(0), rne) == i16_0_t{2});

// 2.4375 * 1.0 + 0.0625 = 2.5 -> 2 after breaking tie
static_assert(fma<i16_0_t, wrap>(i16_4_t::literal(39), i16_4_t{1}, i16_4_t::literal(1), rne) == i16_0_t{2});

// 2.4375 * 1.0 + 0.125 = 2.5625 -> 3
static_assert(fma<i16_0_t, wrap>(i16_4_t::literal(39), i16_4_t{1}, i16_4_t::literal(2), rne) == i16_0_t{3});

// 2.4375 * 1.0 + 1.0625 = 3.5 -> 4 after breaking tie
static_assert(fma<i16_0_t, wrap>(i16_4_t::literal(39), i16_4_t{1}, i16_4_t::literal(17), rne) == i16_0_t{4});

// negative rounding (away from zero / toward zero based on tie-breaking)

// -2.5 -> -2
static_assert(fma<i16_0_t, wrap>(i16_4_t::literal(-39), i16_4_t{1}, i16_4_t::literal(-1), rne) == i16_0_t{-2});

// -3.5 -> -4
static_assert(fma<i16_0_t, wrap>(i16_4_t::literal(-55), i16_4_t{1}, i16_4_t::literal(-1), rne) == i16_0_t{-4});

// unsigned rounding

// 2.4375 * 1.0 + 0.0 = 2.4375 -> rounds to 2
static_assert(fma<u16_0_t, wrap>(u16_4_t::literal(39), u16_4_t{1}, u16_4_t::literal(0), rne) == u16_0_t{2});

// 2.5 * 1.0 + 0.0 = 2.5 -> breaks tie to even (2)
static_assert(fma<u16_0_t, wrap>(u16_4_t::literal(40), u16_4_t{1}, u16_4_t::literal(0), rne) == u16_0_t{2});

// 2.5625 * 1.0 + 0.0 = 2.5625 -> rounds up to nearest (3)
static_assert(fma<u16_0_t, wrap>(u16_4_t::literal(41), u16_4_t{1}, u16_4_t::literal(0), rne) == u16_0_t{3});

// --------------------------------------------------------------------------------------------------------------------
// Scaling
// --------------------------------------------------------------------------------------------------------------------

// upscaling without saturation
// 1.5*2 + 0.5 = 3.5 * (1 << 15) = 114688
static_assert(fma<i32_15_t, wrap>(i16_4_t::literal(24), i16_4_t::literal(32), i16_4_t::literal(8), trunc)
    == i32_15_t::literal(114688));

// upscaling with upper saturation
// 3.0 * 2.0 + 3.0 = 9.0 > 7.999
using i16_12_t = fixed_t<int16_t, 12>;
static_assert(fma<i16_12_t, saturate>(i16_4_t::literal(48), i16_4_t::literal(32), i16_4_t::literal(48), trunc)
    == i16_12_t::literal(max<int16_t>()));

// upscaling with lower saturation
// -3.0 * 2.0 + (-3.0) = -9.0 < -7.999
static_assert(fma<i16_12_t, saturate>(i16_4_t::literal(-48), i16_4_t::literal(32), i16_4_t::literal(-48), trunc)
    == i16_12_t::literal(min<int16_t>()));

// downscaling to pure integer
static_assert(fma<pure_int_t, wrap>(frac_t{1} / 4, frac_t{1} / 4, frac_t{1} / 8, trunc) == pure_int_t{0});

// --------------------------------------------------------------------------------------------------------------------
// Saturation
// --------------------------------------------------------------------------------------------------------------------

// massive overshoots
static_assert(
    fma<i32_15_t, saturate>(i32_15_t{1000}, i32_15_t{1000}, i32_15_t{0}, rne) == literal<i32_15_t>(max<int32_t>()));
static_assert(
    fma<i32_15_t, saturate>(i32_15_t{-1000}, i32_15_t{1000}, i32_15_t{-50}, rne) == literal<i32_15_t>(min<int32_t>()));

// exactly max; no saturation triggered
static_assert(fma<i16_4_t, saturate>(i16_4_t{1}, i16_4_t{1}, i16_4_t::literal(max<int16_t>() - (1 << 4)), trunc)
    == i16_4_t::literal(max<int16_t>()));

// max + 1 ULP; saturates to exactly max
static_assert(fma<i16_4_t, saturate>(i16_4_t{1}, i16_4_t{1}, i16_4_t::literal(max<int16_t>() - (1 << 4) + 1), trunc)
    == i16_4_t::literal(max<int16_t>()));

// exactly min; no saturation triggered
static_assert(fma<i16_4_t, saturate>(i16_4_t{-1}, i16_4_t{1}, i16_4_t::literal(min<int16_t>() + (1 << 4)), trunc)
    == i16_4_t::literal(min<int16_t>()));

// min - 1 ULP; saturates to exactly min
static_assert(fma<i16_4_t, saturate>(i16_4_t{-1}, i16_4_t{1}, i16_4_t::literal(min<int16_t>() + (1 << 4) - 1), trunc)
    == i16_4_t::literal(min<int16_t>()));

// unsigned output saturation; intermediate fits, output does not
static_assert(
    fma<u16_0_t, saturate>(u16_0_t{300}, u16_0_t{300}, u16_0_t{0}, trunc) == u16_0_t::literal(max<uint16_t>()));

// unsigned no-saturation at zero; lower bound stays correct
static_assert(fma<u16_0_t, saturate>(u16_0_t{0}, u16_0_t{300}, u16_0_t{5}, trunc) == u16_0_t{5});

// --------------------------------------------------------------------------------------------------------------------
// Wrapping
// --------------------------------------------------------------------------------------------------------------------

// sum below min of signed output container
static_assert(fma<i16_0_t, wrap>(i16_0_t::literal(-0x100), i16_0_t::literal(0x100), i16_0_t::literal(-0x5), trunc)
    == i16_0_t::literal(-5));

// sum above max of signed output container
static_assert(fma<i16_0_t, wrap>(i16_0_t::literal(0x100), i16_0_t::literal(0x100), i16_0_t::literal(0x5), trunc)
    == i16_0_t::literal(0x5));

// sum above max of unsigned output container
static_assert(fma<u16_0_t, wrap>(u16_0_t::literal(0x100), u16_0_t::literal(0x100), u16_0_t::literal(0x5), trunc)
    == u16_0_t::literal(0x5));

// --------------------------------------------------------------------------------------------------------------------
// Intermediate Accumulator Bounds
// --------------------------------------------------------------------------------------------------------------------

// maximum intermediate expansion: max * max + max; 32-bit inputs require 63 bit internal
static_assert(fma<i32_15_t, saturate>(i32_15_t::literal(max<int32_t>()), i32_15_t::literal(max<int32_t>()),
                  i32_15_t::literal(max<int32_t>()), trunc)
    == i32_15_t::literal(max<int32_t>()));

// minimum intermediate expansion: min * max + min
static_assert(fma<i32_15_t, saturate>(i32_15_t::literal(min<int32_t>()), i32_15_t::literal(max<int32_t>()),
                  i32_15_t::literal(min<int32_t>()), trunc)
    == i32_15_t::literal(min<int32_t>()));

// integer product is exactly offset by large addend before narrowing
using wide_int_t = fixed_t<int32_t, 0>;
using wide_addend_t = fixed_t<int64_t, 0>;
static_assert(fma<i16_4_t, wrap>(
                  wide_int_t::literal(10000), wide_int_t::literal(10000), wide_addend_t::literal(-100000000ll), rne)
    == i16_4_t::literal(0));

// unsigned maximum intermediate expansion
static_assert(fma<u32_15_t, saturate>(u32_15_t::literal(max<uint32_t>()), u32_15_t::literal(max<uint32_t>()),
                  u32_15_t::literal(max<uint32_t>()), trunc)
    == u32_15_t::literal(max<uint32_t>()));

// unsigned minimum, 0
static_assert(
    fma<u32_15_t, saturate>(u32_15_t::literal(0), u32_15_t::literal(max<uint32_t>()), u32_15_t::literal(0), trunc)
    == u32_15_t::literal(0));

// lower bound container wraparound
static_assert(fma<i32_0_t, saturate>(i16_0_t::literal(min<int16_t>()), i16_0_t::literal(max<int16_t>()),
                  i32_0_t::literal(min<int32_t>()), trunc)
    == i32_0_t::literal(min<int32_t>()));

// upper bound container wraparound
static_assert(fma<i32_0_t, saturate>(i16_0_t::literal(min<int16_t>()), i16_0_t::literal(min<int16_t>()),
                  i32_0_t::literal(max<int32_t>()), trunc)
    == i32_0_t::literal(max<int32_t>()));

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
using cascade_mid_t = fixed_t<int64_t, 40>;
using cascade_out_t = fixed_t<int64_t, 20>;
static_assert(fma<cascade_out_t, saturate>(cascade_high_t::literal(1ll << 62), // (2^-18)*2^80
                  cascade_mid_t{1ll << 20}, // (2^20)*2^40
                  cascade_mid_t{19}, // (19)*2^40
                  rne)
    == cascade_out_t::literal((1 << 22) + (19 << 20)));

} // namespace
} // namespace crv
