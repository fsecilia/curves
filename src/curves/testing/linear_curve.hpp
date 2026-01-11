// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Simple curve for testing.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/testing/test.hpp>
#include <ranges>
#include <vector>

namespace curves {

// Simple curve for testing composed curves. Models `f(x) = mx + b`.
template <typename ScalarType>
class LinearCurve {
 public:
  using Scalar = ScalarType;
  using CriticalPoints = std::vector<Scalar>;

  LinearCurve(Scalar m, Scalar b, CriticalPoints critical_points = {}) noexcept
      : m_{m}, b_{b}, critical_points_{std::move(critical_points)} {}

  // Forward: y = mx + b
  template <typename Value>
  auto operator()(Value x) const noexcept -> Value {
    return m_ * x + b_;
  }

  // Inverse: x = (y - b)/m
  template <typename Value>
  auto inverse(Value y) const noexcept -> Value {
    return (y - b_) / m_;
  }

  auto critical_points() const noexcept -> const CriticalPoints& {
    return critical_points_;
  }

  auto critical_points(Scalar domain_max) const noexcept -> auto {
    return critical_points_ | std::views::filter([=](const auto& element) {
             return element <= domain_max;
           }) |
           std::ranges::to<std::vector>();
  }

 private:
  Scalar m_{1.0};
  Scalar b_{0.0};
  CriticalPoints critical_points_{};
};

template <typename Scalar>
auto make_identity(std::vector<Scalar> critical_points = {}) noexcept
    -> LinearCurve<Scalar> {
  return {1.0, 0.0, std::move(critical_points)};
}

template <typename Scalar>
auto make_shift(Scalar offset,
                std::vector<Scalar> critical_points = {}) noexcept
    -> LinearCurve<Scalar> {
  return {1.0, offset, std::move(critical_points)};
}

template <typename Scalar>
auto make_scale(Scalar slope, std::vector<Scalar> critical_points = {}) noexcept
    -> LinearCurve<Scalar> {
  return {slope, 0.0, std::move(critical_points)};
}

}  // namespace curves
