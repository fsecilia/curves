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
class LinearCurve {
 public:
  using CriticalPoints = std::vector<real_t>;

  LinearCurve(real_t m, real_t b, CriticalPoints critical_points = {}) noexcept
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

  auto critical_points(real_t domain_max) const noexcept -> auto {
    return critical_points_ | std::views::filter([=](const auto& element) {
             return element <= domain_max;
           }) |
           std::ranges::to<std::vector>();
  }

 private:
  real_t m_{1.0};
  real_t b_{0.0};
  CriticalPoints critical_points_{};
};

inline auto make_identity(std::vector<real_t> critical_points = {}) noexcept
    -> LinearCurve {
  return {1.0, 0.0, std::move(critical_points)};
}

inline auto make_shift(real_t offset,
                       std::vector<real_t> critical_points = {}) noexcept
    -> LinearCurve {
  return {1.0, offset, std::move(critical_points)};
}

inline auto make_scale(real_t slope,
                       std::vector<real_t> critical_points = {}) noexcept
    -> LinearCurve {
  return {slope, 0.0, std::move(critical_points)};
}

}  // namespace curves
