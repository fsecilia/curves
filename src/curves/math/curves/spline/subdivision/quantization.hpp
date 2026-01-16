// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Explicit quantization steps used during subdivision.

  We construct splines in floating point, then convert to fixed point for
  evaluation. To make sure the same values are evaluated in both formats, we
  quantize in floating point early. All values in our domain at the target
  precisions have are less than 53 bits of mantissa, so quantized doubles are
  bit-exact with the integer representation.

  This module contains facilities for quantizing knots, monomial coefficients
  and segment inverse width to precisions configured by the c evaluator.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <curves/math/jet.hpp>
#include <curves/ranges.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace curves::quantize {

// This value must come from the c acl eventually, but this was built first.
inline constexpr auto kKnotFracBits = 24;

//! Quantizes a knot position to kKnotFracBits.
inline auto knot_position(real_t position) noexcept -> real_t {
  constexpr auto scale = static_cast<real_t>(1LL << kKnotFracBits);
  return std::round(position * scale) / scale;
}

/*!
  Quantizes a coefficient to storage precision.

  Template parameters:
    ImplicitBit - Position of the implicit leading 1 (44, 45, or 46)
    IsSigned    - Whether the coefficient can be negative

  The implicit bit determines the effective precision: ImplicitBit + 1 bits.
*/
template <int ImplicitBit, bool IsSigned = true>
auto coefficient(real_t value) noexcept -> real_t {
  // Handle zero and negative (for unsigned).
  if constexpr (IsSigned) {
    if (value == 0.0) return 0.0;
  } else {
    assert(value >= 0.0);
    if (value <= 0.0) return 0.0;
  }

  int exp;
  std::frexp(value, &exp);

  // Shift places the MSB at the implicit bit position.
  const auto ideal_shift = ImplicitBit - (exp - 1);
  const auto actual_shift = std::clamp(ideal_shift, 0, 62);

  // Scale to integer domain, round, scale back.
  const auto scaled = std::ldexp(value, actual_shift);
  const auto rounded = std::round(scaled);
  const auto result = std::ldexp(rounded, -actual_shift);

  if constexpr (IsSigned) {
    return std::copysign(result, value);
  } else {
    return result;
  }
}

/*
  Convenience aliases matching the storage format.
  These constants must eventually come from the c acl.
*/
inline auto signed_coeff(real_t value) noexcept -> real_t {
  return coefficient<44, true>(value);
}
inline auto unsigned_coeff(real_t value) noexcept -> real_t {
  return coefficient<45, false>(value);
}
inline auto inv_width(real_t value) noexcept -> real_t {
  return coefficient<46, false>(value);
}

inline auto polynomial(const cubic::Monomial& poly) noexcept
    -> cubic::Monomial {
  return cubic::Monomial{
      signed_coeff(poly.coeffs[0]),
      signed_coeff(poly.coeffs[1]),
      unsigned_coeff(poly.coeffs[2]),
      unsigned_coeff(poly.coeffs[3]),
  };
}

}  // namespace curves::quantize
