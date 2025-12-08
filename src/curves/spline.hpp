// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel spline module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cmath>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

extern "C" {
#include <curves/driver/spline.h>
}  // extern "C"

#pragma GCC diagnostic pop

#include <iostream>

namespace curves {

template <typename float_t>
auto calc_max_frac_bits(float_t value) -> int_t {
  int exp;
  std::frexp(value, &exp);
  return exp;
}

template <typename float_t>
auto sign(float_t x) -> float_t {
  static const auto zero = static_cast<float_t>(0);
  static const auto one = static_cast<float_t>(1);

  if (x > zero) return one;
  if (x < zero) return -one;
  return zero;
}

template <typename float_t>
auto to_fixed(float_t value, int_t frac_bits) -> s64 {
  return static_cast<s64>(std::round(std::ldexp(value, frac_bits)));
}

template <typename float_t>
auto to_float(s64 fixed, int_t frac_bits) -> float_t {
  return static_cast<float_t>(fixed) / static_cast<float_t>(1LL << frac_bits);
}

/*
The limit sensitivity(0) = lim_{x→0} v(x)/x = f(0) by L'Hôpital (since v(0) = 0
and v'(x) = f(x)). For sensitivity'(0), if you Taylor expand v(x) = f(0)x +
f'(0)x²/2 + O(x³), then: sensitivity(x) = f(0) + f'(0)x/2 + O(x²)
*/
template <typename Sensitivity>
auto generate_table_from_sensitivity(Sensitivity s, long double x_max)
    -> curves_spline_table {
  const auto frac_bits = 32;

  curves_spline_table result;
  result.max.x = to_fixed(x_max, frac_bits);
  result.max.scale =
      (s64)(((s128)CURVES_SPLINE_TABLE_SIZE << (2 * frac_bits)) / result.max.x);

  const auto dx = x_max / CURVES_SPLINE_TABLE_SIZE;
  const auto eps = dx / 100;

  auto x = 0.0L;
  auto y = s(x);

  // The initial value of m comes from the limit using l'hopital. This is the
  // second order forward difference.
  auto m = (-s(2 * eps) + 4 * s(eps) - 3 * y) * dx / (2 * eps);

  for (auto entry = 0; entry < CURVES_SPLINE_TABLE_SIZE; ++entry) {
    const auto y0 = y;
    const auto m0 = m;

    x += dx;
    y = s(x);

    m = (s(x + eps) - s(x - eps)) * dx / (2 * eps);

    auto& fixed_coeffs = result.coeffs[entry];
    long double float_coeffs[] = {y0, m0, 3 * (y - y0) - 2 * m0 - m,
                                  2 * (y0 - y) + m0 + m};
    for (auto coeff = 0; coeff < CURVES_SPLINE_NUM_COEFS; ++coeff) {
      fixed_coeffs[coeff] = to_fixed(float_coeffs[coeff], frac_bits);
    }
  }

  x += dx;
  y = s(x);
  m = (s(x + eps) - s(x - eps)) * dx / (2 * eps);
  result.max.y = y;
  result.max.m = m;

  return result;
}

template <typename Gain>
auto generate_table_from_gain(Gain g, long double x_max)
    -> curves_spline_table {
  curves_spline_table result;

  auto dx = x_max / CURVES_SPLINE_TABLE_SIZE;
  auto x = 0.0L;
  auto y = g(x);
  auto v = 0.0L;
  auto s = y;

  // The initial value of m comes from the limit using l'hopital. This is the
  // second order forward difference.
  auto m = (-g(2 * dx) + 4 * g(dx) - 3 * y) * dx / (2 * dx);

  int_t min_frac_bits[CURVES_SPLINE_NUM_COEFS] = {64, 64, 64, 64};
  int_t max_frac_bits[CURVES_SPLINE_NUM_COEFS] = {0, 0, 0, 0};

  for (auto entry = 0; entry < CURVES_SPLINE_TABLE_SIZE; ++entry) {
    const auto y0 = y;
    const auto s0 = s;
    const auto m0 = m;

    x += dx;
    y = g(x);

    const auto area = (y + y0) * 0.5L * dx;
    v += area;

    s = v / x;
    m = (y - s) * dx / x;

    auto& fixed_coeffs = result.coeffs[entry];
    long double float_coeffs[] = {s0, m0, 3 * (s - s0) - 2 * m0 - m,
                                  2 * (s0 - s) + m0 + m};
    for (auto coeff = 0; coeff < CURVES_SPLINE_NUM_COEFS; ++coeff) {
      const auto frac_bits = calc_max_frac_bits(float_coeffs[coeff]);
      min_frac_bits[coeff] = std::min(min_frac_bits[coeff], frac_bits);
      max_frac_bits[coeff] = std::max(max_frac_bits[coeff], frac_bits);
      fixed_coeffs[coeff] = to_fixed(float_coeffs[coeff], frac_bits);
      std::cout << float_coeffs[coeff] << " ";
    }
    std::cout << "\n";
    // std::cout << x << ", " << y << std::endl;
  }

  std::cout << "\nmin frac_bits: ";
  for (const auto& frac_bits : min_frac_bits) std::cout << frac_bits << " ";
  std::cout << "\nmax frac_bits: ";
  for (const auto& frac_bits : max_frac_bits) std::cout << frac_bits << " ";
  std::cout << std::endl;

  return result;
}

inline auto synchronous(long double x, long double s, long double p,
                        long double m, long double g, long double k) {
  // clang-format off
  return s * std::exp(
    sign(x - p) * std::log(m) *
    std::pow(
      std::tanh(
        std::pow(
          std::abs((g / std::log(m)) * (std::log(x) - std::log(p))),
          0.5 / k
        )
      ),
      k / 0.5
    )
  );
  // clang-format on
}

}  // namespace curves
