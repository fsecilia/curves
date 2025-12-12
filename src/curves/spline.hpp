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
#include <array>
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

namespace spline {

// Knot to form cubic hermite splines, {x, y, dy/dx}.
struct Knot {
  real_t x;
  real_t y;
  real_t m;
};

/*
  Converts from Hermite form in floating-point:
    H(t) = (2t^3 - 3t^2 + 1)y0 + (t^3 - 2t^2 + t)m0
         + (-2t^3 + 3t^2)y1 + (t^3 - t^2)m1

  To monomial form in fixed-point:
    P(t) = at^3 + bt^2 + ct + d
*/
class SegmentConverter {
 public:
  auto operator()(const Knot& k0, const Knot& k1) const noexcept
      -> curves_spline_segment {
    const auto dx = k1.x - k0.x;
    const auto dy = k1.y - k0.y;
    const auto m0 = k0.m * dx;
    const auto m1 = k1.m * dx;

    return {.coeffs = {Fixed{-2 * dy + m0 + m1}.value,
                       Fixed{3 * dy - 2 * m0 - m1}.value, Fixed{m0}.value,
                       Fixed{k0.y}.value}};
  }
};

// Encapsulates how knots are located.
struct KnotLocator {
  auto operator()(int i) const noexcept -> real_t {
    // Call out to shared c implementation.
    return Fixed::literal(curves_spline_locate_knot(i)).to_real();
  }
};

// Samples a curve to create a knot.
template <typename KnotLocator = KnotLocator>
class KnotSampler {
 public:
  explicit KnotSampler(KnotLocator locator) noexcept
      : locator_{std::move(locator)} {}

  KnotSampler() = default;

  auto operator()(const auto& curve, int_t knot) const -> Knot {
    const auto x = locator_(knot);
    const auto [f, df_dx] = curve(x);
    return {x, f, df_dx};
  }

 private:
  KnotLocator locator_;
};

/*
  Builds a spline by sampling a curve for knots, then building segments
  between the knots.
*/
template <int_t num_segments, typename KnotSampler = KnotSampler<>,
          typename SegmentConverter = SegmentConverter>
class SplineBuilder {
 public:
  SplineBuilder(KnotSampler knot_sampler,
                SegmentConverter segment_converter) noexcept
      : knot_sampler_{std::move(knot_sampler)},
        segment_converter_{std::move(segment_converter)} {}

  SplineBuilder() = default;

  auto operator()(const auto& curve) const noexcept -> curves_spline {
    curves_spline result;

    auto k0 = knot_sampler_(curve, 0);
    for (auto segment = 0; segment < num_segments; ++segment) {
      const auto k1 = knot_sampler_(curve, segment + 1);
      result.segments[segment] = segment_converter_(k0, k1);
      k0 = k1;
    }

    return result;
  }

 private:
  KnotSampler knot_sampler_;
  SegmentConverter segment_converter_;
};

inline auto create_spline(const auto& curve) noexcept -> curves_spline {
  return SplineBuilder<CURVES_SPLINE_NUM_SEGMENTS, KnotSampler<>,
                       SegmentConverter>{}(curve);
}

}  // namespace spline
}  // namespace curves
