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

TEST(spline_coefs, synchronous_as_sensitivity) {
  auto sensitivity = [](long double x) {
    return synchronous(x, 0.433012701892L, 8.3L, 17.3205080757L, 5.33L, 0.5L);
  };

  const auto x_max = 100.0L;
  const auto table = generate_table_from_sensitivity(sensitivity, x_max);

  const auto dx_fixed = to_fixed(1.0e-2L, CURVES_SPLINE_FRAC_BITS);
  const auto x_max_fixed = to_fixed(x_max, CURVES_SPLINE_FRAC_BITS);
  auto x_fixed = s64{0};
  auto max_abs_err = 0.0L;
  auto max_rel_err = 0.0L;
  auto sse_abs = 0.0L;
  auto sse_rel = 0.0L;
  auto num_samples = 0;
  while (x_fixed < x_max_fixed) {
    ++num_samples;
    const auto x_float =
        to_float<long double>(x_fixed, CURVES_SPLINE_FRAC_BITS);
    const auto y_curve = sensitivity(x_float);
    const auto y_table = curves_eval_spline_table(&table, x_fixed);
    x_fixed += dx_fixed;

    const auto expected = y_curve;
    const auto actual = to_float<long double>(y_table, CURVES_SPLINE_FRAC_BITS);

    const auto abs_err = std::abs(actual - expected);
    max_abs_err = std::max(max_abs_err, abs_err);
    sse_abs += abs_err * abs_err;

    const auto rel_err = std::abs(abs_err / expected);
    max_rel_err = std::max(rel_err, max_rel_err);
    sse_rel += rel_err * rel_err;
  }

  const auto mse_abs = sse_abs / num_samples;
  const auto mse_rel = sse_rel / num_samples;
  std::cout << "Max Abs Error: " << max_abs_err << "\nSSE Abs: " << sse_abs
            << "\nMSE Abs: " << mse_abs << "\nRMSE Abs: " << std::sqrt(mse_abs)
            << "\nMax Rel Error: " << max_rel_err << "\nSSE Rel: " << sse_rel
            << "\nMSE Rel: " << mse_rel << "\nRMSE Rel: " << std::sqrt(mse_rel)
            << std::endl;
}

}  // namespace
}  // namespace curves
