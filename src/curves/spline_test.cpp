// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/io.hpp>
#include <curves/spline.hpp>

namespace curves {
namespace {

TEST(spline_set, synchronous_as_transfer_uniform) {
  const auto crossover = 8.3L;
  auto sensitivity = SynchronousCurve{10.0L, 1.0L, crossover, 0.5L};

  const auto spline = create_spline(sensitivity);
  const auto x_max = Fixed::literal(spline.x_max);

  const auto dx = Fixed{1.0e-4L};
  std::cout << "dx: " << dx << " (" << dx.value << " fixed)" << std::endl;

  auto x_fixed = Fixed{0};
  std::cout << "x0: " << x_fixed << " (" << x_fixed.value << " fixed)"
            << std::endl;

  auto max_abs_err = 0.0L;
  auto max_rel_err = 0.0L;
  auto max_abs_err_x = 0.0L;
  auto max_rel_err_x = 0.0L;
  auto sse_abs = 0.0L;
  auto sse_rel = 0.0L;
  auto num_samples = 0;
  while (x_fixed < x_max / 4) {
    const auto x_float = x_fixed.to_real();

    const auto y_curve = x_float * sensitivity(x_float).f;
    const auto y_table = curves_spline_eval(&spline, x_fixed.value);
    x_fixed += dx;

    const auto expected = y_curve;
    const auto actual = Fixed::literal(y_table).to_real();

    ++num_samples;
    if (!expected) continue;  // skip near 0 so rel doesn't explode

    const auto abs_err = std::abs(actual - expected);
    if (max_abs_err < abs_err) {
      max_abs_err = abs_err;
      max_abs_err_x = x_float;
    }
    sse_abs += abs_err * abs_err;

    const auto rel_err = std::abs(abs_err / expected);
    if (max_rel_err < rel_err) {
      max_rel_err = rel_err;
      max_rel_err_x = x_float;
    }
    sse_rel += rel_err * rel_err;
  }
  std::cout << "x1: " << x_fixed << " (" << x_fixed.value << " fixed)"
            << std::endl;

  const auto mse_abs = sse_abs / num_samples;
  const auto mse_rel = sse_rel / num_samples;
  std::cout << "Max Abs Error: " << max_abs_err << " (x = " << max_abs_err_x
            << ")"
            << "\nSSE Abs: " << sse_abs << "\nMSE Abs: " << mse_abs
            << "\nRMSE Abs: " << std::sqrt(mse_abs)
            << "\nMax Rel Error: " << max_rel_err << " (x = " << max_rel_err_x
            << ")"
            << "\nSSE Rel: " << sse_rel << "\nMSE Rel: " << mse_rel
            << "\nRMSE Rel: " << std::sqrt(mse_rel) << std::endl;

  // Trace the warp calculation at x just below x_max
  auto x_test = Fixed{127.9999L};
  s64 x_val = x_test.value;

  s64 log2_alpha_plus_x = approx_log2_q32_q32(spline.alpha + x_val);
  s64 log2_diff = log2_alpha_plus_x - spline.log2_alpha;
  s64 w = (s64)(((s128)log2_diff * spline.inv_log_range) >> 32);
  s64 segment_index = w >> 32;
  s64 t = w & ((1LL << 32) - 1);

  std::cout << "x = " << x_test.to_real() << std::endl;
  std::cout << "log2_alpha_plus_x (Q32) = " << log2_alpha_plus_x << std::endl;
  std::cout << "log2_alpha (Q32) = " << spline.log2_alpha << std::endl;
  std::cout << "log2_diff (Q32) = " << log2_diff << std::endl;
  std::cout << "inv_log_range (Q32) = " << spline.inv_log_range << std::endl;
  std::cout << "w (Q32) = " << w << std::endl;
  std::cout << "segment_index = " << segment_index << std::endl;
  std::cout << "t (Q32) = " << t << " (" << (t / (double)(1LL << 32)) << ")"
            << std::endl;
  std::cout << "Expected t ≈ 0.9999..." << std::endl;
}

}  // namespace
}  // namespace curves
