// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function defined by velocity scale.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/jet.hpp>
#include <limits>
#include <utility>
#include <vector>

namespace curves::transfer_function {

//! Transforms curve to return x*f(x).
template <typename Curve>
class FromVelocityScale {
 public:
  using Scalar = Curve::Scalar;

  explicit FromVelocityScale(Curve curve) noexcept : curve_{std::move(curve)} {}

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
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

  auto critical_points(Scalar domain_max) const -> std::vector<Scalar> {
    return curve_.critical_points(domain_max);
  }

 private:
  Curve curve_;
};

}  // namespace curves::transfer_function
