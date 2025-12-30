// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Composed view of spline and input shaping for curve evaluation.

  Composes InputShapingView and SplineView to produce the four display traces:
    S(v)  = T(u) / v                - sensitivity
    S'(v) = (G - S) / v             - sensitivity derivative
    G(v)  = T'(u)u'                 - gain
    G'(v) = T''(u)(u')^2 + T'(u)u'' - gain derivative

  where u = U(v) is the shaping function.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/input_shaping_view.hpp>
#include <curves/math/spline_view.hpp>
#include <utility>

namespace curves {

struct CurveResult {
  real_t S;   // T(u) / v
  real_t dS;  // (G - S) / v
  real_t G;   // T'(u)u'
  real_t dG;  // T''(u)(u')^2 + T'(u)u''
};

class CurveView {
 public:
  CurveView(InputShapingView shaping, SplineView spline) noexcept
      : shaping_{std::move(shaping)},
        spline_{std::move(spline)},
        u_to_x_{spline_.v_to_x()} {}

  auto valid() const noexcept -> bool { return spline_.valid(); }

  // Evaluate all four display curves at raw velocity v.
  auto operator()(real_t v) const noexcept -> CurveResult {
    // Shaping: v -> u, u', u''
    const auto [u, du, d2u] = shaping_(v);

    // Spline: u -> T, T', T'' (in x-space, need to scale)
    const auto x = u * u_to_x_;
    const auto [T, dT_dx, d2T_dx2] = spline_(x);

    // Scale spline derivatives to u-space
    const auto dT_du = dT_dx * u_to_x_;
    const auto d2T_du2 = d2T_dx2 * u_to_x_ * u_to_x_;

    // Compose: G = (dT/du)(du/dv)
    const auto G = dT_du * du;

    // Compose: G' = (d^2T/du^2)(du/dv)^2 + (dT/du)(d^2u/dv^2)
    const auto dG = d2T_du2 * du * du + dT_du * d2u;

    // S = T/v, S' = (G - S)/v
    constexpr auto kEpsilon = real_t{1e-10};
    if (v < kEpsilon) {
      // At origin: S -> T'(0), S' -> T''(0)/2
      const auto S = dT_du;
      const auto dS = d2T_du2 / 2;
      return {S, dS, G, dG};
    }

    const auto v_inv = 1 / v;
    const auto S = T * v_inv;
    const auto dS = (G - S) * v_inv;

    return {S, dS, G, dG};
  }

  // Evaluate just G(v).
  auto gain(real_t v) const -> real_t {
    const auto [u, du, d2u] = shaping_(v);
    const auto x = u * u_to_x_;
    const auto [T, dT_dx, d2T_dx2] = spline_(x);
    const auto dT_du = dT_dx * u_to_x_;
    return dT_du * du;
  }

  // Evaluate just S(v).
  auto sensitivity(real_t v) const -> real_t {
    auto u = shaping_.eval(v);
    auto x = u * u_to_x_;
    auto T = spline_.eval(x);

    constexpr auto kEpsilon = real_t{1e-10};
    if (v < kEpsilon) {
      auto [T_full, dT_dx, d2T_dx2] = spline_(x);
      return dT_dx * u_to_x_;
    }

    return T / v;
  }

  /* Access to component views */
  auto shaping() const -> const InputShapingView& { return shaping_; }
  auto spline() const -> const SplineView& { return spline_; }

 private:
  InputShapingView shaping_;
  SplineView spline_;
  real_t u_to_x_;
};

}  // namespace curves
