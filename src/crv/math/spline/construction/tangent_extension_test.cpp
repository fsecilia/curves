// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extension.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

namespace extended_tangent_tests {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 25>;
using mantissa_t = int64_t;
using unpacked_field_t = unpacked_field_t<mantissa_t>;

using sut_t = extended_tangent_t<x_t, y_t, unpacked_field_t>;

// constant slope
// y = 0.0*x + 3.0
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}}(x_t{5}) == y_t{3});
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}}(x_t{7}) == y_t{3});

// positive slope
// y = 1.0*x + 0.0
// required shift = 45 - 26 = 19
static_assert(sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}}(x_t{5}) == y_t{5});
static_assert(sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}}(x_t{7}) == y_t{7});

// negative slope with intercept
// y = -0.5*x + 4.0
// required shift = 45 - 25 = 20
static_assert(sut_t{.slope = {.mantissa = -(1LL << 30), .shift = 20}, .y0 = y_t{4}}(x_t{2}) == y_t{3});
static_assert(sut_t{.slope = {.mantissa = -(1LL << 30), .shift = 20}, .y0 = y_t{4}}(x_t{4}) == y_t{2});

} // namespace extended_tangent_tests

} // namespace
} // namespace crv::spline
