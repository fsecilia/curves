// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function adapter and related traits.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curve.hpp>
#include <limits>
#include <utility>

namespace curves {

/*!
  Determines if Curve provides a closed-form antiderivative.
*/
template <typename Curve>
concept CurveHasAntiderivative = requires(const Curve& c, real_t x) {
  { c.antiderivative(c, x) } -> std::convertible_to<real_t>;
};

/*!
  Determines if Traits provides a closed-form antiderivative.

  If a trait provides antiderivative(curve, x) -> real_t, FromGain will use it
  to compute T(x) = F(x) - F(0) analytically instead of using numeric
  integration.
*/
template <typename Traits, typename Curve>
concept TraitsHasAntiderivative =
    requires(const Traits& t, const Curve& c, real_t x) {
      { t.antiderivative(c, x) } -> std::convertible_to<real_t>;
    };

/*!
  Default traits for transfer function computation.

  Assumes the curve is directly evaluable at x = 0. Specialize for curves
  that require limit definitions or have closed-form antiderivatives.
*/
template <typename Curve>
struct TransferFunctionTraits {
  /*!
    Returns the transfer function for the curve at x = 0.

    Both sensitivity and gain interpretations need the curve's output at x = 0
    for the boundary condition. By coincidence, this value is the same
    regardless of which interpretation we use:

      Sensitivity interpretation:
        T(x) = x·S(x)
        T'(x) = S(x) + x·S'(x)  (product rule)
        T'(0) = S(0) + 0·S'(0) = S(0) = curve(0).f

      Gain interpretation:
        T(x) = Int[0, x] G(t) dt
        T'(x) = G(x)
        T'(0) = G(0) = curve(0).f

    The product rule collapses at x = 0, so both paths arrive at the same
    value: the curve's output at the origin. This trait exists only to handle
    curves that require a limit definition instead of direct evaluation.

    \param curve The curve to evaluate.
    \return curve(0).f, or its limit if the curve is singular at 0.
  */
  auto at_0(const Curve& curve) const noexcept -> Jet {
    return {0.0L, curve.value(0.0L)};
  }

  /*!
    Closed-form antiderivative F(x) where F'(x) = G(x).

    If present (detected via TraitsHasAntiderivative concept), FromGain will
    compute T(x) = F(x) - F(0) analytically.
  */
  auto antiderivative(const Curve& curve, real_t x) const noexcept -> real_t
    requires(CurveHasAntiderivative<Curve>)
  {
    return curve.antiderivative(x);
  }
};

template <typename Curve, typename Traits = TransferFunctionTraits<Curve>>
class TransferFunction {
 public:
  auto value(real_t x) const noexcept -> real_t {
    if (x < std::numeric_limits<real_t>::epsilon()) {
      return 0.0L;
    }

    return x * curve_.value(x);
  }

  auto operator()(real_t x) const noexcept -> Jet {
    if (x < std::numeric_limits<real_t>::epsilon()) {
      return traits_.at_0(curve_);
    }

    const auto curve_result = curve_(x);
    return {x * curve_result.f, curve_result.f + x * curve_result.df};
  }

  auto cusp_location() const noexcept -> real_t
    requires HasCusp<Curve>
  {
    return curve_.cusp_location();
  }

  explicit TransferFunction(Curve curve, Traits traits = {}) noexcept
      : curve_{std::move(curve)}, traits_{std::move(traits)} {}

 private:
  Curve curve_;
  Traits traits_;
};

}  // namespace curves
