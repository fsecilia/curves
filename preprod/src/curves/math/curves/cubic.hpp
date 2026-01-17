// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Facilities for evaluating and converting between various basis forms
  of cubic segments.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/jet.hpp>
#include <array>
#include <ostream>
#include <type_traits>

namespace curves::cubic {

constexpr auto coeff_count = 4;

// ----------------------------------------------------------------------------
// Monomial Form
// ----------------------------------------------------------------------------

/*!
  Cubic in Monomial Form

  This form expresses cubics using a monomial basis:

      f(t) = at^3 + bt^2 + ct + d = c[0]t^3 + c[1]t^2 + c[2]t + c[3]

  This form is most expedient for evaluation using Horner's method.
*/
struct Monomial {
  static constexpr auto count = coeff_count;
  std::array<real_t, count> coeffs;

  template <typename T>
  constexpr auto operator()(const T& t) const noexcept -> T {
    using CommonType = std::common_type_t<T, real_t>;
    auto result = static_cast<CommonType>(coeffs[0]);
    return ((result * t + coeffs[1]) * t + coeffs[2]) * t + coeffs[3];
  }

  friend auto operator<<(std::ostream& out, const Monomial& src)
      -> std::ostream& {
    out << "Monomial{" << src.coeffs[0];
    for (auto coeff = 1; coeff < count; ++coeff) {
      out << ", " << src.coeffs[coeff];
    }
    out << "}";
    return out;
  }
};

// ----------------------------------------------------------------------------
// Hermite Form
// ----------------------------------------------------------------------------

/*!
  Converts a cubic Hermite segment to monomial form.

  We represent Hermite segments with jets of their endpoints sampled directly
  from a curve. These jets have value and derivative with respect to the
  domain variable of the curve, (e.g., dy/dx). Since the resulting monomial
  form is normalized to the parameter t in [0, 1], we must apply the chain rule
  (dy/dt = dy/dx*dx/dt) to scale the input derivatives by the segment width.
*/
inline auto hermite_to_monomial(const math::Jet<real_t>& left,
                                const math::Jet<real_t>& right,
                                real_t segment_width) noexcept -> Monomial {
  // Normalize slopes via chain rule.
  const auto m0 = left.v * segment_width;
  const auto m1 = right.v * segment_width;

  // Transform basis.
  return {
      2 * left.a - 2 * right.a + m0 + m1,       // t^3
      -3 * left.a + 3 * right.a - 2 * m0 - m1,  // t^2
      m0,                                       // t
      left.a                                    // 1
  };
}

}  // namespace curves::cubic
