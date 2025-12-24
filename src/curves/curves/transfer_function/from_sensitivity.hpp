// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function adapter for sensitivity curves.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/curves/transfer_function/curve.hpp>
#include <curves/math/curve.hpp>
#include <limits>
#include <utility>

namespace curves {

/*!
  Computes transfer function values from a curve interpreted as sensitivity.

  \tparam Curve The underlying curve type providing {S(x), S'(x)} via
  operator().
  \tparam Traits Handles curve-specific behavior at x = 0.
*/
template <typename Curve, typename Traits = TransferFunctionTraits<Curve>>
class FromSensitivity {
 public:
  explicit FromSensitivity(Curve curve, Traits traits = {}) noexcept
      : curve_{std::move(curve)}, traits_{std::move(traits)} {}

  /*!
    Compute {T(x), G(x)} at the given position.

    \param x Position to evaluate.
    \return {T(x), G(x)} where T(x) = xS(x) and G(x) = T'(x).
  */
  auto operator()(real_t x) noexcept -> Jet {
    if (x < std::numeric_limits<real_t>::epsilon()) {
      // Evaluate curve indirectly.
      return traits_.at_0(curve_);
    }

    // Evaluate curve directly.
    const auto [S, dS] = curve_(x);

    // By definition, T(x) = xS(x)
    const real_t T = x * S;

    // By the product rule, G(x) = T'(x) = S(x) + xS'(x)
    const real_t G = S + x * dS;

    return {T, G};
  }

  auto cusp_location() const noexcept -> real_t
    requires HasCusp<Curve>
  {
    return curve_.cusp_location();
  }

  //! Access the underlying curve.
  auto curve() const noexcept -> const Curve& { return curve_; }

 private:
  Curve curve_;
  Traits traits_;
};

}  // namespace curves
