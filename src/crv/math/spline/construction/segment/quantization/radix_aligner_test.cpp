// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "radix_aligner.hpp"
#include <crv/math/float_extraction.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using mantissa_t = int32_t;
using scaled_int_t = scaled_int_t<mantissa_t>;

struct unpacked_field_t
{
    mantissa_t mantissa;
    int_t shift;
    constexpr auto operator==(unpacked_field_t const&) const noexcept -> bool = default;
};

// instantiate the aligner with arbitrary safe bounds for our 32-bit test container
constexpr auto aligner = exponent_aligner_t<-20, 20>{};
constexpr auto align_radix = radix_aligner_t<unpacked_field_t, scaled_int_t, aligner>{};

// passthrough; exponent is well within the aligner's bounds
// exponent = 5 + 2 = 7, shift output becomes -7, mantissa is untouched
static_assert(align_radix({.mantissa = 100, .exponent = 5}, 2) == unpacked_field_t{.mantissa = 100, .shift = -7});

// positive saturation; exponent exceeds max
// exponent = 15 + 10 = 25, clamps to 20
// deficit of 5 means mantissa is left-shifted by 5 (10 << 5 = 320); shift output is -20
static_assert(align_radix({.mantissa = 10, .exponent = 15}, 10) == unpacked_field_t{.mantissa = 320, .shift = -20});

// negative saturation; exponent falls below min
// exponent = -15 + (-10) = -25, clamps to -20
// surplus of 5 means mantissa is right-shifted by 5 (1000 >> 5 = 31 RNE); shift output is 20
static_assert(align_radix({.mantissa = 1000, .exponent = -15}, -10) == unpacked_field_t{.mantissa = 31, .shift = 20});

} // namespace
} // namespace crv::spline
