// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/spline/construction/segment/field_packer.hpp>
#include <crv/math/spline/construction/segment/segment_factory.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using traits_t = traits_t<unpacked_field_t<int_t>>;

using packed_field_t = traits_t::packed_field_t;
using unpacked_field_t = traits_t::unpacked_field_t;
using mantissa_t = traits_t::mantissa_t;
using packed_segment_t = traits_t::packed_segment_t;
using unpacked_segment_t = traits_t::unpacked_segment_t;

using field_layout_t = field_layout_t<packed_field_t>;
using field_packer_t = field_packer_t<packed_field_t>;
using field_unpacker_t = field_unpacker_t<unpacked_field_t>;
using segment_layout_t = segment_layout_t<field_layout_t>;

// ====================================================================================================================
// layouts
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// field_layout_t
// --------------------------------------------------------------------------------------------------------------------

//
// shift mask
//

// unsigned boundaries
static_assert(field_layout_t{0, false}.shift_mask() == 0x0ULL);
static_assert(field_layout_t{1, false}.shift_mask() == 0x1ULL);
static_assert(field_layout_t{8, false}.shift_mask() == 0xffULL);
static_assert(field_layout_t{63, false}.shift_mask() == 0x7fffffffffffffffULL);

// signed boundaries
static_assert(field_layout_t{0, true}.shift_mask() == 0x0ULL);
static_assert(field_layout_t{1, true}.shift_mask() == 0x1ULL);
static_assert(field_layout_t{8, true}.shift_mask() == 0xffULL);

//
// min shift
//

// unsigned min is always 0
static_assert(field_layout_t{0, false}.min_shift() == 0);
static_assert(field_layout_t{63, false}.min_shift() == 0);

// signed min scales by powers of two
static_assert(field_layout_t{0, true}.min_shift() == 0x0);
static_assert(field_layout_t{1, true}.min_shift() == -0x1);
static_assert(field_layout_t{8, true}.min_shift() == -0x80);
static_assert(field_layout_t{63, true}.min_shift() == -0x4000000000000000LL);

//
// max shift
//

// unsigned max matches the mask
static_assert(field_layout_t{0, false}.max_shift() == 0x0);
static_assert(field_layout_t{1, false}.max_shift() == 0x1);
static_assert(field_layout_t{8, false}.max_shift() == 0xff);

// signed max is positive half of the domain
static_assert(field_layout_t{0, true}.max_shift() == -0x1); // empty domain
static_assert(field_layout_t{1, true}.max_shift() == 0x0);
static_assert(field_layout_t{8, true}.max_shift() == 0x7f);
static_assert(field_layout_t{63, true}.max_shift() == 0x3fffffffffffffffLL);

// ====================================================================================================================
// unpacking
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// field_unpacker_t
// --------------------------------------------------------------------------------------------------------------------

namespace field_unpacker_tests {

constexpr auto test_unpack(unpacked_field_t unpacked_field, field_layout_t field_layout) noexcept -> bool
{
    static constexpr auto pack_field = field_packer_t{};
    static constexpr auto unpack_field = field_unpacker_t{};

    auto const packed_field = pack_field(unpacked_field, field_layout);

    auto const actual = unpack_field(packed_field, field_layout);

    return unpacked_field == actual;
}

// positive value, unsigned shift
static_assert(test_unpack({.mantissa = 5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = false}));

// negative value, unsigned shift
static_assert(test_unpack({.mantissa = -5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = false}));

// positive value, positive shift
static_assert(test_unpack({.mantissa = 5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// negative value, positive shift
static_assert(test_unpack({.mantissa = -5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// positive value, negative shift
static_assert(test_unpack({.mantissa = 5, .shift = -3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// negative value, negative shift
static_assert(test_unpack({.mantissa = -5, .shift = -3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// separate configuration
static_assert(test_unpack({.mantissa = 10, .shift = 5}, field_layout_t{.shift_width = 3, .is_signed = false}));

} // namespace field_unpacker_tests

// --------------------------------------------------------------------------------------------------------------------
// segment_unpacker_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_unpacker_tests {

constexpr auto intermediate_layout_shift = 5;
constexpr auto final_layout_shift = 9;
constexpr auto segment_layout = segment_layout_t{
    .intermediate = field_layout_t{.shift_width = intermediate_layout_shift, .is_signed = false},
    .final = field_layout_t{.shift_width = final_layout_shift, .is_signed = true},
};

struct echoing_field_unpacker_t
{
    using unpacked_field_t = unpacked_field_t;

    constexpr auto operator()(packed_field_t packed_field, field_layout_t layout) const noexcept -> unpacked_field_t
    {
        // echo input into mantissa, and fingerprint layout into shift
        return {.mantissa = static_cast<mantissa_t>(packed_field), .shift = layout.shift_width};
    }
};

constexpr auto unpacker
    = segment_unpacker_t<packed_segment_t, unpacked_segment_t, echoing_field_unpacker_t, segment_layout>{};
constexpr auto packed_segment = packed_segment_t{10, 20, 30, 40};

//
// routing tests
//

// fields 0, 1, 2 get the intermediate layout
static_assert(unpacker(packed_segment, 0) == unpacked_field_t{.mantissa = 10, .shift = intermediate_layout_shift});
static_assert(unpacker(packed_segment, 1) == unpacked_field_t{.mantissa = 20, .shift = intermediate_layout_shift});
static_assert(unpacker(packed_segment, 2) == unpacked_field_t{.mantissa = 30, .shift = intermediate_layout_shift});

// field 3 gets the final layout
static_assert(unpacker(packed_segment, 3) == unpacked_field_t{.mantissa = 40, .shift = final_layout_shift});

// full segment
constexpr auto unpacked_array = unpacker(packed_segment);
static_assert(unpacked_array[0] == unpacked_field_t{.mantissa = 10, .shift = intermediate_layout_shift});
static_assert(unpacked_array[1] == unpacked_field_t{.mantissa = 20, .shift = intermediate_layout_shift});
static_assert(unpacked_array[2] == unpacked_field_t{.mantissa = 30, .shift = intermediate_layout_shift});
static_assert(unpacked_array[3] == unpacked_field_t{.mantissa = 40, .shift = final_layout_shift});

} // namespace segment_unpacker_tests

// ====================================================================================================================
// evaluation
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// segment_evaluator_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_evaluator_tests {

using narrow_t = int32_t;
using x_t = fixed_t<narrow_t, 14>;
using y_t = fixed_t<narrow_t, 18>;
constexpr auto evaluate = segment_evaluator_t<traits_t, x_t, y_t>{};

//
// zero and constant
//

// x = 0
static_assert(evaluate({{{0, 0}, {0, 0}, {0, 0}, {11, 0}}}, x_t::literal(0)) == y_t::literal(11));

// x != 0; x shouldn't affect a constant
static_assert(evaluate({{{0, 0}, {0, 0}, {0, 0}, {11, 0}}}, x_t::literal(10)) == y_t::literal(11));

//
// linear
//

// 7*10 = 70
static_assert(evaluate({{{0, 0}, {0, 0}, {7, 0}, {0, 0}}}, x_t::literal(10)) == y_t::literal(70));

//
// quadratic
//

// 5*10^2 = 500
static_assert(evaluate({{{0, 0}, {5, 0}, {0, 0}, {0, 0}}}, x_t::literal(10)) == y_t::literal(500));

//
// cubic
//

// 3*100^3 = 3000
static_assert(evaluate({{{3, 0}, {0, 0}, {0, 0}, {0, 0}}}, x_t::literal(10)) == y_t::literal(3000));

//
// mixed
//

// no shift
// 3*100^2 + 5*10^2 + 7*10 + 11 = 3581
static_assert(evaluate({{{3, 0}, {5, 0}, {7, 0}, {11, 0}}}, x_t::literal(10)) == y_t::literal(3581));

// no shift, negative
// -3*(-10)^3 + -5*(-10)^2 + -7*(-10) + -11 = 3000 - 500 + 70 - 11 = 2559
static_assert(evaluate({{{-3, 0}, {-5, 0}, {-7, 0}, {-11, 0}}}, x_t::literal(-10)) == y_t::literal(2559));

// with shifts
// y = 3592
constexpr auto y_expected
    = ((((((3 * 1024 * 10 >> 1) + 5 * 512) * 10 >> 2) + 7 * 128) * 10) + (11 * 32 << 3)) >> (3 + 4);
static_assert(evaluate({{{3 * 1024, 1}, {5 * 512, 2}, {7 * 128, 3}, {11 * 32, 4}}}, x_t::literal(10))
    == y_t::literal(y_expected));

//
// saturation boundaries
//

// saturate positive after c3
static_assert(
    evaluate({{{0, 0}, {0, 0}, {0, 0}, {mantissa_t{max<narrow_t>()} + 1, 0}}}, x_t::literal(1)) == max<y_t>());

// saturate negative after c3
static_assert(
    evaluate({{{0, 0}, {0, 0}, {0, 0}, {mantissa_t{min<narrow_t>()} - 1, 0}}}, x_t::literal(1)) == min<y_t>());

//
// dynamic alignment
//

// precision reduction / right shift
//
// c = 5, d = 12, x = 1.0 (16384 in 14-bit fixed)
// sum and shift right by 1:
//  - multiply = 5 * 16384 = 81920
//  - align d, d <<= 14 = 12 << 14 = 196608
//  - sum = 81920 + 196608 = 278528
//  - total_left_shift must be -1:
//      - (seg[2].shift + seg[3].shift) = -1
//      - (14 + -13) = -1
//  - 278528 >> 1 = 139264
static_assert(evaluate({{{0, 0}, {0, 0}, {5, 14}, {12, -13}}}, x_t{1}) == y_t::literal(139264));

// precision gain / left shift
//
// c = 5, d = 12, x = 1.0 (16384 in 14-bit fixed)
// sum and shift right by 1:
//  - multiply = 5 * 16384 = 81920
//  - align d, d <<= 14 = 12 << 14 = 196608
//  - sum = 81920 + 196608 = 278528
//  - total_left_shift must be 2:
//      - (seg[2].shift + seg[3].shift) = 2
//      - (14 + -16) = 2
//  - 278528 << 2 = 1114112
static_assert(evaluate({{{0, 0}, {0, 0}, {5, 14}, {12, -16}}}, x_t{1}) == y_t::literal(1114112));

} // namespace segment_evaluator_tests

// ====================================================================================================================
// orchestration
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// segment_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_tests {

//
// configure
//

// pipeline
using narrow_t = int32_t;
using x_t = fixed_t<narrow_t, 14>;
using y_t = fixed_t<narrow_t, 18>;
constexpr auto segment_layout = segment_layout_t{
    .intermediate = {.shift_width = 4, .is_signed = false},
    .final = {.shift_width = 4, .is_signed = true},
};

using unpacker_t = segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
using evaluator_t = segment_evaluator_t<traits_t, x_t, y_t>;
using sut_t = segment_t<traits_t, x_t, unpacker_t, evaluator_t>;

// test abi and memory invariants
static_assert(std::is_trivially_copyable_v<sut_t>);
static_assert(alignof(sut_t) >= 32);

//
// create sut from nontrivial segment
//

constexpr auto x = 10;
constexpr auto a = 3 * 1024;
constexpr auto a_shift = 1;
constexpr auto b = 5 * 512;
constexpr auto b_shift = 2;
constexpr auto c = 7 * 128;
constexpr auto c_shift = 3;
constexpr auto d = 11 * 32;
constexpr auto d_shift = 4;
constexpr int_t y_expected
    = ((((((a * x >> a_shift) + b) * x >> b_shift) + c) * x) + (d << c_shift)) >> (c_shift + d_shift);

constexpr auto pack(mantissa_t mantissa, int_t shift) noexcept -> packed_field_t
{
    auto const shift_mask = segment_layout.intermediate.shift_mask();
    return (static_cast<packed_field_t>(mantissa) << segment_layout.intermediate.shift_width)
        | (static_cast<packed_field_t>(shift) & shift_mask);
}
constexpr auto packed_segment
    = packed_segment_t{pack(a, a_shift), pack(b, b_shift), pack(c, c_shift), pack(d, d_shift)};

constexpr auto sut = sut_t{packed_segment};

//
// test
//

static_assert(sut(x_t::literal(x)) == y_t::literal(y_expected));

} // namespace segment_tests

} // namespace
} // namespace crv::spline
