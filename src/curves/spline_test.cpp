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

  const auto frac_bits = 32;
  const auto dx_fixed = to_fixed(1.0e-1L, frac_bits);
  const auto x_max_fixed = to_fixed(x_max, frac_bits);
  auto x_fixed = s64{0};
  while (x_fixed < x_max_fixed) {
    const auto x_float = to_float<long double>(x_fixed, frac_bits);
    const auto y_curve = sensitivity(x_float);
    const auto y_table = curves_eval_spline_table(table, x_fixed, frac_bits);
    std::cout << x_float << " " << y_curve << " "
              << to_float<long double>(y_table, frac_bits) << std::endl;
    x_fixed += dx_fixed;
  }
}

}  // namespace
}  // namespace curves
