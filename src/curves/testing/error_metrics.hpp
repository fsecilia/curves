// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Methods for measuring statistical error.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/testing/test.hpp>
#include <ostream>

namespace curves {

struct AccuracyMetrics {
  real_t max_abs_err = 0.0L;
  real_t max_rel_err = 0.0L;
  real_t max_abs_err_x = 0.0L;
  real_t max_rel_err_x = 0.0L;
  real_t sse_abs = 0.0L;
  real_t sse_rel = 0.0L;
  int num_samples = 0;

  auto mse_abs() const -> real_t { return sse_abs / num_samples; }
  auto mse_rel() const -> real_t { return sse_rel / num_samples; }
  auto rmse_abs() const -> real_t { return std::sqrt(mse_abs()); }
  auto rmse_rel() const -> real_t { return std::sqrt(mse_rel()); }

  auto sample(real_t x, real_t actual, real_t expected) noexcept -> void {
    ++num_samples;

    // Skip near zero so rel doesn't explode.
    if (std::abs(expected) < 1e-12L) return;

    // Accumulate absolute error.
    const auto abs_err = std::abs(actual - expected);
    if (abs_err > max_abs_err) {
      max_abs_err = abs_err;
      max_abs_err_x = x;
    }
    sse_abs += abs_err * abs_err;

    // Accumulate relative error.
    const auto rel_err = abs_err / std::abs(expected);
    if (rel_err > max_rel_err) {
      max_rel_err = rel_err;
      max_rel_err_x = x;
    }
    sse_rel += rel_err * rel_err;
  }

  friend auto operator<<(std::ostream& out, const AccuracyMetrics& src)
      -> std::ostream& {
    // clang-format off
    return out 
      << "Samples: " << src.num_samples << "\n"
      << "Max Abs Error: " << src.max_abs_err 
        << " (x = " << src.max_abs_err_x << ")\n"
      << "RMSE Abs: " << src.rmse_abs() << "\n"
      << "Max Rel Error: " << src.max_rel_err 
        << " (x = " << src.max_rel_err_x << ")\n"
      << "RMSE Rel: " << src.rmse_rel();
    // clang-format on
  }
};

}  // namespace curves
