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

template <CurveDefinition kDefinition, typename Curve>
class TransferFunction;

/*!
  This specialization expects to parameterize on an antiderivative of a curve,
  which it invokes directly.
*/
template <typename Antiderivative>
class TransferFunction<CurveDefinition::kTransferGradient, Antiderivative> {
 public:
  using Scalar = Antiderivative::Scalar;

  explicit TransferFunction(Antiderivative antiderivative) noexcept
      : antiderivative_{std::move(antiderivative)} {}

  template <typename Value>
  auto operator()(Value v) const noexcept -> Value {
    return antiderivative_(v);
  }

 private:
  Antiderivative antiderivative_;
};

/*!
  Transforms curve to return x*f(x).
*/
template <typename Curve>
class TransferFunction<CurveDefinition::kVelocityScale, Curve> {
 public:
  using Scalar = Curve::Scalar;

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

template <typename AntiderivativeBuilder>
struct TransferFunctionBuilder {
  [[no_unique_address]] AntiderivativeBuilder antiderivative_builder;

  /*
    Since the transfer function type varies with the enum, this uses a
    CPS-style visitor that is invoked with the completed transfer function.
    This way, the spline builder doesn't need to know anything about how the
    transfer function is built.
  */
  template <typename Curve, typename Visitor>
  auto operator()(CurveDefinition curve_definition, Curve curve,
                  Curve::Scalar max, Curve::Scalar tolerance,
                  CompatibleRange<typename Curve::Scalar> auto critical_points,
                  Visitor&& visitor) -> decltype(auto) {
    switch (curve_definition) {
      case CurveDefinition::kTransferGradient: {
        // Create an antiderivative wrapper for the curve.
        auto antiderivative = antiderivative_builder(
            std::move(curve), max, tolerance, critical_points);
        using Antiderivative = decltype(antiderivative);
        return visitor(
            TransferFunction<CurveDefinition::kTransferGradient,
                             Antiderivative>{std::move(antiderivative)});
      }

      case CurveDefinition::kVelocityScale: {
        // Use the curve directly;
        return visitor(TransferFunction<CurveDefinition::kVelocityScale, Curve>(
            std::move(curve)));
      }
    };
  }
};

}  // namespace curves
