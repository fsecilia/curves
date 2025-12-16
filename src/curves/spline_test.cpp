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
  auto sensitivity = SynchronousCurve{0.433012701892L, 10.0L, 1.0L, 8.3L, 0.5L};

  const auto spline = spline::create_spline(TransferAdapterCurve{sensitivity});
  const auto x_max =
      Fixed::literal(spline::locate_knot(SPLINE_NUM_SEGMENTS - 1));

  const auto dx = Fixed{1.0e-3L};
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
  while (x_fixed < x_max) {
    const auto x_float = x_fixed.to_real();

    const auto y_curve = x_float * sensitivity(x_float).f;
    const auto y_table = spline::eval(&spline, x_fixed.value);
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
}

}  // namespace
}  // namespace curves
