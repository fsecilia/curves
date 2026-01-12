// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Facilities for evaluating and converting between various basis forms
  of cubic segments.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
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
template <typename ScalarType>
struct Monomial {
  using Scalar = ScalarType;

  Scalar coeffs[coeff_count];

  template <typename T>
  constexpr auto operator()(const T& t) const noexcept -> T {
    using CommonType = std::common_type_t<T, Scalar>;
    auto result = static_cast<CommonType>(coeffs[0]);
    return ((result * t + coeffs[1]) * t + coeffs[2]) * t + coeffs[3];
  }

  friend auto operator<<(std::ostream& out, const Monomial& src)
      -> std::ostream& {
    out << "Monomial{" << src.coeffs[0];
    for (auto coeff = 1; coeff < coeff_count; ++coeff) {
      out << ", " << src.coeffs[coeff];
    }
    out << "}";
    return out;
  }
};

}  // namespace curves::cubic
