// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/spline/construction/segment/field_packer.hpp>
#include <crv/spline/construction/segment/segment_packer.hpp>
#include <crv/test/test.hpp>
#include <type_traits>

namespace crv::spline {
namespace {

using layout_y_t = fixed_t<int64_t, 20>;
using layout_traits_t = spline::traits_t<spline::unpacked_field_t<int_t>, layout_y_t>;
using packed_field_t = layout_traits_t::packed_field_t;
using unpacked_field_t = layout_traits_t::unpacked_field_t;
using mantissa_t = layout_traits_t::mantissa_t;
using packed_segment_t = layout_traits_t::packed_segment_t;
using unpacked_segment_t = layout_traits_t::unpacked_segment_t;
using field_layout_t = spline::field_layout_t<packed_field_t>;
using field_packer_t = spline::field_packer_t<packed_field_t>;
using field_unpacker_t = spline::field_unpacker_t<unpacked_field_t>;
using segment_layout_t = spline::segment_layout_t<field_layout_t>;

namespace field_layout_tests {

static_assert(field_layout_t{0, false}.shift_mask() == 0x0ULL);
static_assert(field_layout_t{8, false}.shift_mask() == 0xffULL);
static_assert(field_layout_t{8, true}.min_shift() == -0x80);
static_assert(field_layout_t{8, true}.max_shift() == 0x7f);

} // namespace field_layout_tests

namespace field_unpacker_tests {

constexpr auto test_unpack(unpacked_field_t unpacked_field, field_layout_t field_layout) noexcept -> bool
{
    auto const packed = field_packer_t{}(unpacked_field, field_layout);
    return field_unpacker_t{}(packed, field_layout) == unpacked_field;
}

static_assert(test_unpack({.mantissa = 5, .shift = 3}, {.shift_width = 4, .is_signed = false}));
static_assert(test_unpack({.mantissa = -5, .shift = -3}, {.shift_width = 4, .is_signed = true}));

} // namespace field_unpacker_tests

namespace segment_unpacker_tests {

constexpr auto intermediate_shift_width = 5;
constexpr auto final_shift_width = 9;
constexpr auto segment_layout = segment_layout_t{
    .intermediate = {.shift_width = intermediate_shift_width, .is_signed = false},
    .final = {.shift_width = final_shift_width, .is_signed = true},
};

struct echoing_field_unpacker_t
{
    using unpacked_field_t = spline::unpacked_field_t<int_t>;

    constexpr auto operator()(packed_field_t packed_field, field_layout_t layout) const noexcept -> unpacked_field_t
    {
        return {.mantissa = static_cast<mantissa_t>(packed_field), .shift = layout.shift_width};
    }
};

constexpr auto unpacker
    = segment_unpacker_t<packed_segment_t, unpacked_segment_t, echoing_field_unpacker_t, segment_layout>{};
constexpr auto g0 = layout_y_t::literal(40);
constexpr auto packed = packed_segment_t{.d = 10, .c = 20, .b = 30, .g0 = g0};

static_assert(unpacker(packed, 0) == unpacked_field_t{.mantissa = 10, .shift = intermediate_shift_width});
static_assert(unpacker(packed, 1) == unpacked_field_t{.mantissa = 20, .shift = intermediate_shift_width});
static_assert(unpacker(packed, 2) == unpacked_field_t{.mantissa = 30, .shift = final_shift_width});

constexpr auto unpacked = unpacker(packed);
static_assert(unpacked.d == unpacked_field_t{.mantissa = 10, .shift = intermediate_shift_width});
static_assert(unpacked.c == unpacked_field_t{.mantissa = 20, .shift = intermediate_shift_width});
static_assert(unpacked.b == unpacked_field_t{.mantissa = 30, .shift = final_shift_width});
static_assert(unpacked.g0 == g0);

constexpr auto packer = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>{};
constexpr auto real_unpacker
    = segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>{};
constexpr auto roundtrip_source = unpacked_segment_t{
    .d = {.mantissa = 1234567, .shift = 17},
    .c = {.mantissa = -2345678, .shift = 9},
    .b = {.mantissa = 3456789, .shift = -37},
    .g0 = layout_y_t::literal(-4567890),
};
static_assert(real_unpacker(packer(roundtrip_source)) == roundtrip_source);

static_assert(sizeof(packed_segment_t) == 32);
static_assert(std::is_trivially_copyable_v<packed_segment_t>);

} // namespace segment_unpacker_tests

namespace segment_evaluator_tests {

// Deliberately different fixed formats. The correction product has y_frac+x_frac and division by x back to y has
// compile-time radix shift x_frac-(y_frac+x_frac)+y_frac == 0.
using x_t = fixed_t<int32_t, 7>;
using y_t = fixed_t<int64_t, 19>;
using traits_t = spline::traits_t<spline::unpacked_field_t<int64_t>, y_t>;
using unpacked_segment_t = traits_t::unpacked_segment_t;
constexpr auto evaluate = segment_evaluator_t<traits_t, x_t, y_t>{};
static_assert(decltype(evaluate)::correction_divide_shift == 0);

template <typename value_t> constexpr auto constant_s(value_t s_raw, value_t g0_raw) noexcept -> unpacked_segment_t
{
    return {
        .d = {.mantissa = 0, .shift = 0},
        .c = {.mantissa = 0, .shift = 0},
        .b = {.mantissa = static_cast<int64_t>(s_raw), .shift = 0},
        .g0 = y_t::literal(static_cast<int64_t>(g0_raw)),
    };
}

// First segment: x==0 is valid and no division occurs.
static_assert(evaluate(constant_s(11, 99), x_t::literal(0), x_t::literal(0)) == y_t::literal(11));
static_assert(evaluate(constant_s(11, 99), x_t::literal(7), x_t::literal(0)) == y_t::literal(11));

// At a nonzero segment origin x==x0, the induced identity returns g0 exactly.
static_assert(evaluate(constant_s(10, 14), x_t::literal(3), x_t::literal(3)) == y_t::literal(14));

// Positive, negative, and zero corrections.
static_assert(evaluate(constant_s(10, 14), x_t::literal(6), x_t::literal(3)) == y_t::literal(12));
static_assert(evaluate(constant_s(14, 10), x_t::literal(6), x_t::literal(3)) == y_t::literal(12));
static_assert(evaluate(constant_s(12, 12), x_t::literal(6), x_t::literal(3)) == y_t::literal(12));

// x >> x0 keeps the correction bounded and small.
static_assert(evaluate(constant_s(10, 110), x_t::literal(100), x_t::literal(1)) == y_t::literal(11));

// Explicit nearest-away division: +/- 1/2 raw output unit both round away from zero, not toward zero.
static_assert(evaluate(constant_s(10, 11), x_t::literal(2), x_t::literal(1)) == y_t::literal(11));
static_assert(evaluate(constant_s(10, 9), x_t::literal(2), x_t::literal(1)) == y_t::literal(9));

TEST(segment_evaluator_test, malformed_extreme_ordinates_use_wrapping_subtraction_and_addition)
{
    auto const x = x_t::literal(1);
    auto const max_y = max<typename y_t::value_t>();
    auto const min_y = min<typename y_t::value_t>();

    EXPECT_EQ(evaluate(constant_s(max_y, min_y), x, x), y_t::literal(min_y));
    EXPECT_EQ(evaluate(constant_s(min_y, max_y), x, x), y_t::literal(max_y));
}

} // namespace segment_evaluator_tests

namespace segment_safety_tests {

using x_t = fixed_t<int64_t, 0>;
using y_t = fixed_t<int64_t, 0>;
using traits_t = spline::traits_t<spline::unpacked_field_t<int64_t>, y_t>;
using unpacked_segment_t = traits_t::unpacked_segment_t;
constexpr auto sut = segment_evaluator_t<traits_t, x_t, y_t>{};
constexpr auto narrow_min = min<int64_t>();
constexpr auto narrow_max = max<int64_t>();

constexpr auto make_segment(int64_t d, int_t d_shift, int64_t c, int_t c_shift, int64_t b, int_t b_shift,
    int64_t g0 = 0) noexcept -> unpacked_segment_t
{
    return {
        .d = {.mantissa = d, .shift = d_shift},
        .c = {.mantissa = c, .shift = c_shift},
        .b = {.mantissa = b, .shift = b_shift},
        .g0 = y_t::literal(g0),
    };
}

// zero local coordinate still validates coefficient additions and final alignment
static_assert(sut.is_safe_through(make_segment(narrow_max, 0, narrow_min, 0, 0, 0), x_t::literal(0), x_t{0}));

// signed minimum is handled as a value, not by taking an unrepresentable signed absolute value
static_assert(sut.is_safe_through(make_segment(narrow_min, 0, 0, 0, 0, 0), x_t::literal(1), x_t{0}));

// largest positive product and widest right-shift bias fit the wide intermediate
static_assert(sut.is_safe_through(
    make_segment(narrow_max, 127, 0, 127, 0, 0), x_t::literal(narrow_max), x_t{0}));

// stage narrowing boundary and one-past coefficient addition
static_assert(sut.is_safe_through(make_segment(narrow_max, 0, 0, 0, 0, 0), x_t::literal(1), x_t{0}));
static_assert(!sut.is_safe_through(make_segment(narrow_max, 0, 1, 0, 0, 0), x_t::literal(1), x_t{0}));

// first Horner stage can fit while the second overflows its mathematical accumulator
static_assert(!sut.is_safe_through(
    make_segment(1, 0, narrow_max - 1, 0, 1, 0), x_t::literal(1), x_t{0}));

// mandatory subdivision can turn an unsafe quadratic envelope into a safe one
constexpr auto quadratic = make_segment(1, 0, 0, 0, 0, 0);
static_assert(!sut.is_safe_through(quadratic, x_t::literal(3'037'000'500), x_t{0}));
static_assert(sut.is_safe_through(quadratic, x_t::literal(1'518'500'250), x_t{0}));

// deliberate final saturation and in-range signed left shifts are safe
static_assert(sut.is_safe_through(make_segment(0, 0, 0, 0, 1, -63), x_t::literal(1), x_t{0}));
static_assert(sut.is_safe_through(make_segment(0, 0, 0, 0, -1, -1), x_t::literal(1), x_t{0}));
static_assert(!sut.is_safe_through(make_segment(0, 0, 0, 0, narrow_max, -127), x_t::literal(1), x_t{0}));
static_assert(sut.is_safe_through(make_segment(0, 0, 0, 0, -1, -127), x_t::literal(1), x_t{0}));

// dynamic shift extrema are accepted when arithmetic permits them and rejected when the count is invalid
static_assert(sut.is_safe_through(make_segment(0, 127, 0, 127, 0, -127), x_t::literal(1), x_t{0}));
static_assert(!sut.is_safe_through(make_segment(0, 128, 0, 0, 0, 0), x_t::literal(1), x_t{0}));
static_assert(!sut.is_safe_through(make_segment(0, 0, 0, 0, 0, 128), x_t::literal(1), x_t{0}));

// correction subtraction must fit y_t before the bounded x0/x correction can execute
static_assert(sut.is_safe_through(make_segment(0, 0, 0, 0, narrow_min, 0, narrow_max), x_t::literal(1), x_t{0}));
static_assert(!sut.is_safe_through(make_segment(0, 0, 0, 0, narrow_min, 0, narrow_max), x_t::literal(1), x_t{1}));

// interval endpoint itself must be representable
static_assert(!sut.is_safe_through(make_segment(0, 0, 0, 0, 0, 0), x_t::literal(1), x_t::literal(narrow_max)));

} // namespace segment_safety_tests

namespace segment_tests {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 18>;
using traits_t = spline::traits_t<spline::unpacked_field_t<int64_t>, y_t>;
using packed_segment_t = traits_t::packed_segment_t;
using unpacked_segment_t = traits_t::unpacked_segment_t;
using packed_field_t = traits_t::packed_field_t;
using field_layout_t = spline::field_layout_t<packed_field_t>;
using segment_layout_t = spline::segment_layout_t<field_layout_t>;
constexpr auto segment_layout = segment_layout_t{
    .intermediate = {.shift_width = 4, .is_signed = false},
    .final = {.shift_width = 4, .is_signed = true},
};
using field_unpacker_t = spline::field_unpacker_t<traits_t::unpacked_field_t>;
using unpacker_t = segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
using evaluator_t = segment_evaluator_t<traits_t, x_t, y_t>;
using sut_t = segment_t<traits_t, x_t, unpacker_t, evaluator_t>;

constexpr auto pack_field = spline::field_packer_t<packed_field_t>{};
constexpr auto packed = packed_segment_t{
    .d = pack_field(typename traits_t::unpacked_field_t{.mantissa = 0, .shift = 0}, segment_layout.intermediate),
    .c = pack_field(typename traits_t::unpacked_field_t{.mantissa = 0, .shift = 0}, segment_layout.intermediate),
    .b = pack_field(typename traits_t::unpacked_field_t{.mantissa = 7, .shift = 0}, segment_layout.final),
    .g0 = y_t::literal(11),
};
constexpr auto sut = sut_t{packed};

static_assert(sizeof(sut_t) == 32);
static_assert(alignof(sut_t) == 32);
static_assert(std::is_trivially_copyable_v<sut_t>);
static_assert(sut(x_t::literal(2), x_t::literal(2)) == y_t::literal(11));
static_assert(sut.is_safe_through(x_t::literal(4), x_t::literal(2)));

} // namespace segment_tests

} // namespace
} // namespace crv::spline
