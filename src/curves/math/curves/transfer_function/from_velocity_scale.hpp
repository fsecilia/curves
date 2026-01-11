// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function defined by velocity scale.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
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
  auto operator()(const Value& x) const noexcept -> Value {
    return x * curve_(x);
  }

  auto critical_points(Scalar domain_max) const -> std::vector<Scalar> {
    return curve_.critical_points(domain_max);
  }

 private:
  Curve curve_;
};

}  // namespace curves::transfer_function
