// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extension.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/spline/segment.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 25>;
using mantissa_t = int64_t;
using unpacked_field_t = unpacked_field_t<mantissa_t>;

using sut_t = extended_tangent_t<x_t, y_t, unpacked_field_t>;

constexpr auto x_inf = max<x_t>();

// safety proof
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}.is_safe());
static_assert(!sut_t{.slope = {.mantissa = -1, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}.is_safe());
static_assert(!sut_t{.slope = {.mantissa = 1, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_t::literal(-1)}.is_safe());
static_assert(!sut_t{.slope = {.mantissa = 1, .shift = 128}, .y0 = y_t{3}, .x_max_delta = x_t{1}}.is_safe());
static_assert(!sut_t{.slope = {.mantissa = 1, .shift = min<int_t>()}, .y0 = y_t{3}, .x_max_delta = x_t{1}}.is_safe());

// right-shift narrowing boundary: max*max needs 63 bits of right shift to fit int64
using integer_x_t = fixed_t<int64_t, 0>;
using integer_y_t = fixed_t<int64_t, 0>;
using integer_sut_t = extended_tangent_t<integer_x_t, integer_y_t, unpacked_field_t>;
using truncating_integer_sut_t
    = extended_tangent_t<integer_x_t, integer_y_t, unpacked_field_t, rounding_modes::shr::truncate>;
constexpr auto integer_max = max<int64_t>();
static_assert(integer_sut_t{
    .slope = {.mantissa = integer_max, .shift = 63},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal(integer_max),
}
        .is_safe());
static_assert(!integer_sut_t{
    .slope = {.mantissa = integer_max, .shift = 62},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal(integer_max),
}
        .is_safe());

// rounding mode controls runtime right shifts
static_assert(truncating_integer_sut_t{
                  .slope = {.mantissa = 3, .shift = 1},
                  .y0 = integer_y_t{},
                  .x_max_delta = integer_x_t{1},
              }(integer_x_t{1})
    == integer_y_t{1});

// exact left-shift boundary and one raw unit beyond it
static_assert(integer_sut_t{
    .slope = {.mantissa = 1, .shift = -1},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal(integer_max >> 1),
}
        .is_safe());
static_assert(!integer_sut_t{
    .slope = {.mantissa = 1, .shift = -1},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal((integer_max >> 1) + 1),
}
        .is_safe());

// constant slope
// y = 0.0*x + 3.0
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}(x_t{5}) == y_t{3});
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}(x_t{7}) == y_t{3});

// positive slope
// y = 1.0*x + 0.0
// required shift = 45 - 26 = 19
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{5}) == y_t{5});
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{7}) == y_t{7});

// y = 1.0*x + 0.0, represented with an exact left shift at runtime.
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 10, .shift = -1}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{5}) == y_t{5});

// y = 1.0*x + 0.0, shift = 19. Clamp at x=5.
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_t{5}}(x_t{3}) == y_t{3});
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_t{5}}(x_t{10}) == y_t{5});

// clamp distance is derived from the stored quantized line
static_assert(sut_t::clamp_delta({.mantissa = 1LL << 30, .shift = 19}, y_t{3}, y_t{5}) == x_t{2});
static_assert(sut_t::clamp_delta({.mantissa = 1LL << 10, .shift = -1}, y_t{3}, y_t{5}) == x_t{2});
static_assert(sut_t::clamp_delta({.mantissa = 0, .shift = 0}, y_t{3}, y_t{5}) == x_inf);
static_assert(sut_t::clamp_delta({.mantissa = 1, .shift = 0}, y_t{5}, y_t{5}) == x_t{0});

// bounded evaluator reaches, but never numerically crosses, upper output bound
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{3}, .x_max_delta = x_t{2}}(x_t{3}) == y_t{5});

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(tangent_extension_test_t, rejects_negative_slope)
{
    EXPECT_DEATH(static_cast<void>(sut_t::clamp_delta({.mantissa = -1, .shift = 0}, y_t{3}, y_t{5})), "slope.mantissa");
}

#endif // defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
