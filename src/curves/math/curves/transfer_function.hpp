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

namespace curves {

template <CurveDefinition kDefinition, typename Curve>
class TransferFunction;

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
    // Use limit definition of the derivative near v = 0.
    //
    // For T(v) = v * S(v), the product rule gives:
    //   T'(v) = S(v) + v * S'(v)
    //
    // `S'(v)` may diverge at 0 (e.g. for root functions), but the divergence
    // for our specific set of curves is strictly slower than `O(1/v)`. The
    // linear scaling `v` damps the singularity, causing the term to vanish:
    //   lim[v->0] (v * S'(v)) = 0
    //
    // This leaves a finite limit for the derivative:
    //   T'(0) = S(0) + 0 = S(0)
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

}  // namespace curves
