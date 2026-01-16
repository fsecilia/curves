// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Top-level transfer function. This is what the spline approximates.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/math/jet.hpp>
#include <curves/ranges.hpp>

namespace curves {

template <typename Integral>
concept IsIntegral = requires(const Integral integral, real_t scalar) {
  { integral(scalar) } -> std::convertible_to<real_t>;
  { integral.integrand()(scalar) } -> std::convertible_to<real_t>;
};

template <CurveDefinition kDefinition, typename Curve>
class TransferFunction;

/*!
  Parameterize on an integral.

  This specialization invokes the integral directly on the scalar path. On the
  jet path, it cracks the jet to evaluate the integral as a scalar, then uses
  the integral's original integrand to calculate the derivative.
*/
template <IsIntegral Integral>
class TransferFunction<CurveDefinition::kTransferGradient, Integral> {
 public:
  explicit TransferFunction(Integral integral) noexcept
      : integral_{std::move(integral)} {}

  // Scalar path returns integral sum directly.
  template <math::IsNotJet Value>
  auto operator()(Value v) const noexcept -> Value {
    return integral_(v);
  }

  // Jet path applies chain rule using ftoc.
  template <math::IsJet Jet>
  auto operator()(const Jet& v) const -> Jet {
    using math::derivative;
    using math::primal;

    const auto v_primal = primal(v);
    const auto v_derivative = derivative(v);

    // Integrate normally using the jet's primal.
    const auto integral = integral_(v_primal);

    /*
      By the Fundamental Theorem of Calculus, the integrand of an integral is
      the integral's derivative: `d/dx Int[0->x] f(t) dt = f(x)`
      Evaluate it directly and apply the chain rule.
    */
    const auto integrand = integral_.integrand();
    const auto integral_derivative = integrand(v_primal);

    return math::Jet<real_t>{integral, integral_derivative * v_derivative};
  }

 private:
  Integral integral_;
};

/*!
  Transforms curve to return x*f(x).
*/
template <typename Curve>
class TransferFunction<CurveDefinition::kVelocityScale, Curve> {
 public:
  explicit TransferFunction(Curve curve) noexcept : curve_{std::move(curve)} {}

  template <typename Value>
  auto operator()(Value v) const noexcept -> Value {
    /*
      Use limit definition of the derivative near v = 0.

      For T(v) = v * S(v), the product rule gives:
        T'(v) = S(v) + v * S'(v)

      `S'(v)` may diverge at 0 (e.g. for root functions), but the divergence
      for our specific set of curves is strictly slower than `O(1/v)`. The
      linear scaling `v` damps the singularity, causing the term to vanish:
        lim[v->0] (v * S'(v)) = 0

      This leaves a finite limit for the derivative:
        T'(0) = S(0) + 0 = S(0)
    */
    if constexpr (math::IsJet<Value>) {
      if (v < std::numeric_limits<Value>::epsilon()) {
        return Value{0.0, curve_(0.0) * derivative(v)};
      }
    }

    return v * curve_(v);
  }

 private:
  Curve curve_;
};

template <typename CachedIntegralBuilder, typename NumericalIntegralFactory>
struct TransferFunctionBuilder {
  [[no_unique_address]] CachedIntegralBuilder cached_integral_builder;
  [[no_unique_address]] NumericalIntegralFactory numerical_integral_factory;

  /*
    Since the transfer function type varies with the enum, this uses a
    CPS-style visitor that is invoked with the completed transfer function.
  */
  template <typename Curve, typename Visitor>
  auto operator()(CurveDefinition curve_definition, Curve curve, real_t max,
                  real_t tolerance, ScalarRange auto critical_points,
                  Visitor&& visitor) const -> decltype(auto) {
    switch (curve_definition) {
      case CurveDefinition::kTransferGradient: {
        // Compose curve with numerical integrator in a single callable.
        auto integral_adapter = numerical_integral_factory(std::move(curve));

        // Compose callable integral with adaptive quadrature cache.
        auto cached_integral = cached_integral_builder(
            std::move(integral_adapter), max, tolerance, critical_points);

        // Compose TransferFunction over cached integral.
        using CachedIntegral = decltype(cached_integral);
        return visitor(
            TransferFunction<CurveDefinition::kTransferGradient,
                             CachedIntegral>{std::move(cached_integral)});
      }

      case CurveDefinition::kVelocityScale: {
        // No integration needed. Pass the curve directly.
        return visitor(TransferFunction<CurveDefinition::kVelocityScale, Curve>{
            std::move(curve)});
      }
    }
    std::unreachable();
  }
};

}  // namespace curves
