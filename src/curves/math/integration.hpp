// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Methods of integration.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <array>

namespace curves {

/*!
  Results of f(x) and f'(x).

  This will eventually support autodifferentiation using dual numbers, but for
  now, it's just the function and its derivative.
*/
struct Jet {
  real_t f;
  real_t fp;
};

// ----------------------------------------------------------------------------
// Hermite-based quadrature (uses endpoint derivatives)
// ----------------------------------------------------------------------------

/*!
  Fixed size grid for 8th order Hermite-corrected trapezoid. It divides the
  interval sequentially into quarters and includes both endpoints.
*/
using TrapezoidGrid8 = std::array<Jet, 5>;

/*!
  Hermite-corrected trapezoidal rule (Euler-Maclaurin correction).

  Uses endpoint values and derivatives to achieve O(h^4) accuracy without any
  interior evaluations.

    S[a, b] f(x)dx ~= (h/2)(f(a) + f(b)) - (h^2/12)(f'(b) - f'(a))

  \param h Interval width (b - a).
  \param a Function and derivative results at left endpoint.
  \param b Function and derivative results at right endpoint.
  \return Approximate value of S[a, b] f(x)dx with O(h^4) accuracy.
*/
inline auto corrected_trapezoid(real_t h, const Jet& a, const Jet& b) noexcept
    -> real_t {
  constexpr auto c0 = 0.5L;
  constexpr auto c1 = 1.0L / 12.0L;
  return (c0 * (a.f + b.f) + c1 * (a.fp - b.fp) * h) * h;
}

/*!
  Hermite-corrected trapezoidal rule (Euler-Maclaurin correction) with two
  levels of Richardson extrapolation.

  Computes at h, h/2, and h/4, then extrapolates twice to O(h^8).

  \param h Interval width (b - a).
  \return Approximate value of S[a, b] F(x)dx with O(h^8) accuracy.
*/
inline auto corrected_trapezoid(real_t h, const TrapezoidGrid8& p) noexcept
    -> real_t {
  // Full step.
  const auto I_h = corrected_trapezoid(h, p[0], p[4]);

  // Half steps.
  const auto h_2 = h * 0.5L;
  const auto I_h2 = corrected_trapezoid(h_2, p[0], p[2]) +
                    corrected_trapezoid(h_2, p[2], p[4]);

  // Quarter steps.
  const auto h_4 = h * 0.25L;
  const auto I_h4 = corrected_trapezoid(h_4, p[0], p[1]) +
                    corrected_trapezoid(h_4, p[1], p[2]) +
                    corrected_trapezoid(h_4, p[2], p[3]) +
                    corrected_trapezoid(h_4, p[3], p[4]);

  // First Richardson: O(h^4) -> O(h^6)
  constexpr auto c_r1 = 1.0L / 15.0L;
  const auto R1_h = c_r1 * (16.0L * I_h2 - I_h);
  const auto R1_h2 = c_r1 * (16.0L * I_h4 - I_h2);

  // Second Richardson: O(h^6) -> O(h^8)
  constexpr auto c_r2 = 1.0L / 63.0L;
  return c_r2 * (64.0L * R1_h2 - R1_h);
}

/*!
  Hermite-corrected trapezoidal rule (Euler-Maclaurin correction) with two
  levels of Richardson extrapolation.

  Computes at h, h/2, and h/4, then extrapolates twice to O(h^8).
  Requires three additional evaluations (midpoint and quarter points).

  \param curve Callable returning {f(x), f'(x)}.
  \param a Left endpoint.
  \param b Right endpoint.
  \param f_a Function value at left endpoint.
  \param f_b Function value at right endpoint.
  \param df_a Derivative at left endpoint.
  \param df_b Derivative at right endpoint.
  \return Approximate value of S[a, b] f(x)dx with O(h^8) accuracy.
*/
template <typename Curve>
auto corrected_trapezoid(const Curve& f, real_t a, real_t b, real_t f_a,
                         real_t f_b, real_t df_a, real_t df_b) noexcept
    -> real_t {
  const auto q1 = 0.25L * (3 * a + b);
  const auto [f_q1, df_q1] = f(q1);

  const auto mid = 0.5L * (a + b);
  const auto [f_mid, df_mid] = f(mid);

  const auto q3 = 0.25L * (a + 3 * b);
  const auto [f_q3, df_q3] = f(q3);

  const auto h = b - a;
  const auto grid = TrapezoidGrid8{{
      {f_a, df_a},
      {f_q1, df_q1},
      {f_mid, df_mid},
      {f_q3, df_q3},
      {f_b, df_b},
  }};
  return corrected_trapezoid(h, grid);
}

// ----------------------------------------------------------------------------
// Gauss-Legendre quadrature (does not use derivatives)
// ----------------------------------------------------------------------------

/*!
  3-point Gauss-Legendre quadrature.

  Exact for polynomials up to degree 5, O(h^6) error for smooth functions.
  Evaluates the integrand at 3 interior points; no endpoint derivatives needed.

  Use this when derivatives are unavailable or unreliable.

  \param f Callable returning the integrand value at a point.
  \param a Lower bound of integration.
  \param b Upper bound of integration.
  \return Approximate value of S[a, b] f(x)dx.
*/
template <typename F>
auto gauss3(F&& f, real_t a, real_t b) noexcept -> real_t {
  // Nodes: (+/-)(3/5)^(1/2), 0
  // Weights: 5/9, 8/9, 5/9
  constexpr auto s = 0.7745966692414834L;  // (3/5)^(1/2)
  constexpr auto w_outer = 5.0L / 9.0L;
  constexpr auto w_center = 8.0L / 9.0L;

  const auto mid = 0.5L * (a + b);
  const auto half = 0.5L * (b - a);

  const auto f0 = f(mid - s * half).f;
  const auto f1 = f(mid).f;
  const auto f2 = f(mid + s * half).f;

  return half * (w_outer * f0 + w_center * f1 + w_outer * f2);
}

auto gauss4(auto&& f, real_t a, real_t b) noexcept -> real_t {
  constexpr auto n0 = 0.861136311594052575224L;
  constexpr auto n1 = 0.339981043584856264803L;
  constexpr auto w0 = 0.347854845137453857373L;
  constexpr auto w1 = 0.652145154862546142627L;

  const auto mid = 0.5L * (a + b);
  const auto half = 0.5L * (b - a);

  const auto f0 = f(mid - half * n0).f;
  const auto f1 = f(mid + half * n0).f;
  const auto f2 = f(mid - half * n1).f;
  const auto f3 = f(mid + half * n1).f;

  return half * (w0 * (f0 + f1) + w1 * (f2 + f3));
}

/*!
  5-point Gauss-Legendre quadrature.

  Exact for polynomials up to degree 9, O(h^10) error for smooth functions.
  Higher accuracy than gauss3 at the cost of 2 additional function evaluations.

  \param f Callable returning the integrand value at a point.
  \param a Lower bound of integration.
  \param b Upper bound of integration.
  \return Approximate value of S[a, b] f(x)dx.
*/
template <typename F>
auto gauss5(F&& f, real_t a, real_t b) noexcept -> real_t {
  // Nodes and weights for 5-point Gauss-Legendre on [-1, 1]
  // constexpr auto x0 = 0.0L;
  constexpr auto x1 = 0.5384693101056831L;  // (5 - 2(10/7)^(1/2))^(1/2) / 3
  constexpr auto x2 = 0.9061798459386640L;  // (5 + 2(10/7)^(1/2))^(1/2) / 3

  constexpr auto w0 = 0.5688888888888889L;  // 128/225
  constexpr auto w1 = 0.4786286704993665L;  // (322 + 13(70)^(1/2)) / 900
  constexpr auto w2 = 0.2369268850561891L;  // (322 - 13(70)^(1/2)) / 900

  const auto mid = 0.5L * (a + b);
  const auto half = 0.5L * (b - a);

  return half * (w0 * f(mid) + w1 * (f(mid - x1 * half) + f(mid + x1 * half)) +
                 w2 * (f(mid - x2 * half) + f(mid + x2 * half)));
}

}  // namespace curves
