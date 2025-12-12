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

struct Knot {
  real_t x;
  real_t y;
  real_t m;
};

using Knots = std::vector<Knot>;

inline real_t knot_sample_location(int i) {
  return Fixed::literal(curves_spline_calc_knot_x(i)).to_real();
}

inline auto create_knots(const auto& curve, int_t num_knots) -> Knots {
  if (!num_knots) return {};

  auto knots = Knots{};
  knots.reserve(num_knots);
  for (auto i = 0; i < num_knots; ++i) {
    const auto x = knot_sample_location(i);
    auto result = curve(x);
    knots.emplace_back(x, result.f, result.df_dx);
  }

  return knots;
}

// Evaluates a pair of hermite knots to create a cubic segment between them.
inline auto convert_segment(const auto& p0, const auto& p1,
                            s64* coeffs) noexcept -> void {
  const auto dx = p1->x - p0->x;
  const auto dy = p1->y - p0->y;
  const auto m0 = p0->m * dx;
  const auto m1 = p1->m * dx;

  const auto a = -2 * dy + m0 + m1;
  const auto b = 3 * dy - 2 * m0 - m1;
  const auto c = m0;
  const auto d = p0->y;

  coeffs[0] = Fixed{a}.value;
  coeffs[1] = Fixed{b}.value;
  coeffs[2] = Fixed{c}.value;
  coeffs[3] = Fixed{d}.value;
}

inline auto create_spline(const auto& curve) noexcept -> curves_spline {
  curves_spline result;

  const auto knots =
      create_knots(TransferAdapterCurve{curve}, CURVES_SPLINE_NUM_SEGMENTS + 1);

  auto* p0 = knots.data();
  auto* p1 = p0;
  for (auto segment = 0; segment < CURVES_SPLINE_NUM_SEGMENTS; ++segment) {
    p0 = p1++;
    convert_segment(*p0, *p1, result.coeffs[segment]);
  }

  return result;
}

}  // namespace curves
