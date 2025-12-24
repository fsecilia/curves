// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function adapter for Gain curves.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/curves/transfer_function/curve.hpp>
#include <curves/math/curve.hpp>
#include <curves/math/integration.hpp>
#include <utility>

namespace curves {

/*!
  Computes transfer function values from a curve interpreted as gain.

  \tparam Curve The underlying curve type providing {G(x), G'(x)}.
  \tparam Traits Provides boundary condition and optional antiderivative.

  This frankenclass should be split into analytic and numeric versions.
*/
template <typename Curve, typename Traits = TransferFunctionTraits<Curve>>
class FromGain {
 public:
  explicit FromGain(Curve curve, Traits traits = {}) noexcept
      : curve_{std::move(curve)}, traits_{std::move(traits)} {}

  /*!
    Compute {T(x), G(x)} at the given position.

    \pre x >= previous x (monotonic advancement required for numeric
         integration). For analytic integration this constraint is not
         enforced but following it ensures consistent behavior.

    \param x Position to evaluate.
    \return {T(x), G(x)} where T(x) = int[0, x] G(t)dt.
  */
  auto operator()(real_t x) noexcept -> Jet {
    if constexpr (TraitsHasAntiderivative<Traits, Curve>) {
      return advance_analytic(x);
    } else {
      return advance_numeric(x);
    }
  }

  auto cusp_location() const noexcept -> real_t
    requires HasCusp<Curve>
  {
    return curve_.cusp_location();
  }

  //! Access to the underlying curve.
  auto curve() const noexcept -> const Curve& { return curve_; }

  //! Current accumulated transfer function value.
  auto transfer() const noexcept -> real_t { return T_; }

  //! Current position in the integration.
  auto position() const noexcept -> real_t { return x_; }

 private:
  Curve curve_;
  Traits traits_;

  // Accumulator state for numeric integration.
  real_t T_ = 0.0L;
  real_t x_ = 0.0L;

  //! Compute {T(x), G(x)} using the closed-form antiderivative.
  auto advance_analytic(real_t x) noexcept -> Jet
    requires(TraitsHasAntiderivative<Traits, Curve>)
  {
    // T(x) = F(x) - F(0) where F' = G
    const auto T = traits_.antiderivative(curve_, x) -
                   traits_.antiderivative(curve_, 0.0L);
    const auto G = curve_.value(x);
    return {T, G};
  }

  //! Compute {T(x), G(x)} using numeric integration.
  auto advance_numeric(real_t x) noexcept -> Jet {
    if (x > x_) {
      // Integrate from previous position using selected method.
      T_ += integrate(x_, x);

      // Update state for next segment.
      x_ = x;
    }

    return {T_, curve_.value(x)};
  }

  auto integrate(real_t a, real_t b) const noexcept -> real_t {
    return gauss5(curve_, a, b);
  }
};

}  // namespace curves
