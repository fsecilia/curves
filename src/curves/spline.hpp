// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel spline module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

extern "C" {
#include <curves/driver/spline.h>
}  // extern "C"

#pragma GCC diagnostic pop

#include <curves/lib.hpp>
#include <curves/fixed.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

namespace curves {

struct CurveResult {
  real_t f;
  real_t df_dx;
};

class SynchronousCurve {
 public:
  SynchronousCurve(real_t motivity, real_t gamma, real_t sync_speed,
                   real_t smooth)
      : motivity_{motivity},
        L{std::log(motivity)},
        g{gamma / L},
        p{sync_speed},
        k{smooth == 0 ? 16.0 : 0.5 / smooth},
        r{1.0 / k} {}

  auto motivity() const noexcept -> real_t { return motivity_; }

  auto operator()(real_t x) const noexcept -> CurveResult {
    if (std::abs(x - p) <= std::numeric_limits<real_t>::epsilon()) {
      return {1.0, L * g / p};
    }

    if (x > p) {
      return evaluate<+1>(g * (std::log(x) - std::log(p)), x);
    } else {
      return evaluate<-1>(g * (std::log(p) - std::log(x)), x);
    }
  }

 private:
  real_t motivity_;

  real_t L;  // log(motivity)
  real_t g;  // gamma / L
  real_t p;  // sync_speed
  real_t k;  // sharpness = 0.5 / smooth
  real_t r;  // 1 / sharpness

  // 'sign' is +1 for x > p, -1 for x < p.
  // It only affects the exponent of f; the derivative formula is invariant.
  template <int sign>
  auto evaluate(real_t u, real_t x) const noexcept -> CurveResult {
    // Shared intermediate terms
    real_t u_pow_k_minus_1 = std::pow(u, k - 1);
    real_t u_pow_k = u_pow_k_minus_1 * u;  // v = u^k

    real_t w = std::tanh(u_pow_k);  // w = tanh(v)
    real_t w_pow_r_minus_1 = std::pow(w, r - 1);
    real_t w_pow_r = w_pow_r_minus_1 * w;  // z = w^r

    real_t sech_sq = 1 - w * w;  // sech(v)^2

    // Forward: f = exp((+/-)L * z)
    real_t f = std::exp(sign * L * w_pow_r);

    // Derivative: df/dx = (f * L * g / x) * u^(k-1) * w^(r-1) * sech(v)^2
    real_t df_dx =
        (f * L * g / x) * u_pow_k_minus_1 * w_pow_r_minus_1 * sech_sq;

    return {f, df_dx};
  }
};

template <typename Curve>
struct TransferAdapterTraits {
  auto eval_at_0(const Curve& curve) const noexcept -> CurveResult {
    return {0, curve(0.0).f};
  }
};

template <typename Curve, typename Traits = TransferAdapterTraits<Curve>>
class TransferAdapterCurve {
 public:
  auto operator()(real_t x) const noexcept -> CurveResult {
    if (x < std::numeric_limits<real_t>::epsilon()) {
      return traits_.eval_at_0(curve_);
    }

    const auto curve_result = curve_(x);
    return {x * curve_result.f, curve_result.f + x * curve_result.df_dx};
  }

  explicit TransferAdapterCurve(Curve curve, Traits traits = {}) noexcept
      : curve_{std::move(curve)}, traits_{std::move(traits)} {}

 private:
  Curve curve_;
  Traits traits_;
};

template <>
struct TransferAdapterTraits<SynchronousCurve> {
  auto eval_at_0(const SynchronousCurve& curve) const noexcept -> CurveResult {
    /*
      This comes from the limit definition of the derivative of the transfer
      function.
    */
    return {0.0, 1.0 / curve.motivity()};
  }
};

// C++ version of the kernel's approximate log2, for use during knot generation
inline double approx_log2_corrected(double x) {
  if (x <= 0) return 0;

  int n = static_cast<int>(std::floor(std::log2(x)));
  double normalized = x / std::exp2(n);  // In range [1, 2)
  double f = normalized - 1.0;           // In range [0, 1)

  // Minimax polynomial from Sollya
  // double f_corrected = f * (1.4116228283382952 + f * (-0.43316044507082552));

  // Must match kernel coefficients exactly
  constexpr double coeff_a = 1.4426950408889634;  // 1/ln(2)
  constexpr double coeff_b = 0.4426950408889634;  // 1/ln(2) - 1
  double f_corrected = f * (coeff_a - f * coeff_b);

  return n + f_corrected;
}

inline double approx_log2_as_kernel(double x) {
  s64 x_q32 = static_cast<s64>(x * (1LL << 32));
  s64 result_q32 = approx_log2_q32_q32(x_q32);
  return static_cast<double>(result_q32) / (1LL << 32);
}

inline double approx_warp(double x, double alpha, double inv_log_range,
                          double log2_alpha) {
  return (approx_log2_as_kernel(alpha + x) - log2_alpha) * inv_log_range;
}

// Find x such that approx_warp(x) = target, using bisection
inline double invert_approx_warp(double target, double alpha, double x_max,
                                 double inv_log_range, double log2_alpha) {
  double lo = 0, hi = x_max;
  for (int iter = 0; iter < 64;
       ++iter) {  // 64 iterations gives ~18 decimal digits
    double mid = (lo + hi) / 2;
    if (approx_warp(mid, alpha, inv_log_range, log2_alpha) < target)
      lo = mid;
    else
      hi = mid;
  }
  return (lo + hi) / 2;
}

struct Knot {
  real_t x;
  real_t y;
  real_t m;
};

using Knots = std::vector<Knot>;

auto create_knots(const auto& curve, real_t x_max, real_t alpha,
                  int_t num_knots) -> Knots {
  if (!num_knots) return {};

  // Precompute constants using the SAME approximate log2
  const auto log2_alpha = approx_log2_as_kernel(alpha);
  const auto log2_alpha_plus_xmax = approx_log2_as_kernel(alpha + x_max);
  const auto log_range = log2_alpha_plus_xmax - log2_alpha;
  const auto inv_log_range = (num_knots - 1) / log_range;

  auto knots = Knots{};
  knots.reserve(num_knots);

  for (auto i = 0; i < num_knots; ++i) {
    const auto x =
        invert_approx_warp(i, alpha, x_max, inv_log_range, log2_alpha);
    auto result = curve(x);
    knots.emplace_back(x, result.f, result.df_dx);
  }

  return knots;
}

inline auto create_spline(const auto& curve) noexcept -> curves_spline {
  curves_spline result;

  static constexpr auto x_max = real_t{CURVES_SPLINE_DOMAIN_END_INT};
  static constexpr auto alpha = x_max / 4;
  const auto num_knots = CURVES_SPLINE_NUM_SEGMENTS + 1;
  const auto knots =
      create_knots(TransferAdapterCurve{curve}, x_max, alpha, num_knots);

  auto* p0 = knots.data();
  auto* p1 = p0;
  for (auto segment = 0; segment < CURVES_SPLINE_NUM_SEGMENTS; ++segment) {
    p0 = p1++;

    const auto dx = p1->x - p0->x;
    real_t m0 = p0->m * dx;
    real_t m1 = p1->m * dx;
    real_t y0 = p0->y;
    real_t y1 = p1->y;

    auto a = 2 * y0 - 2 * y1 + m0 + m1;
    auto b = 3 * (y1 - y0) - 2 * m0 - m1;
    auto c = m0;
    auto d = y0;

    s64* coeffs = result.coeffs[segment];
    coeffs[0] = Fixed{a}.value;
    coeffs[1] = Fixed{b}.value;
    coeffs[2] = Fixed{c}.value;
    coeffs[3] = Fixed{d}.value;
  }

  const auto log2_alpha = approx_log2_as_kernel(alpha);
  const auto log2_alpha_plus_xmax = approx_log2_as_kernel(alpha + x_max);
  const auto log_range = log2_alpha_plus_xmax - log2_alpha;

  result.alpha = Fixed{alpha}.value;
  result.log2_alpha = Fixed{log2_alpha}.value;
  result.inv_log_range = Fixed{CURVES_SPLINE_NUM_SEGMENTS / log_range}.value;
  result.x_max = Fixed{x_max}.value;
  result.final_segment_inv_width = Fixed{1.0 / (p1->x - p0->x)}.value;

  return result;
}

}  // namespace curves
