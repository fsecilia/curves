// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Methods of integration.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curve.hpp>
#include <array>

namespace curves {

// Sample arrays for each quadrature method.
using Trapezoid4Samples = std::array<Jet, 2>;  // 2 endpoints
using Trapezoid8Samples = std::array<Jet, 5>;  // endpoints + 3 interior
using Gauss3Samples = std::array<real_t, 3>;
using Gauss4Samples = std::array<real_t, 4>;
using Gauss5Samples = std::array<real_t, 5>;

// ----------------------------------------------------------------------------
// Concepts
// ----------------------------------------------------------------------------

template <typename F>
concept JetCallable = requires(F f, real_t x) {
  { f(x) } -> std::convertible_to<Jet>;
};

template <typename F>
concept ValueCallable = requires(F f, real_t x) {
  { f(x) } -> std::convertible_to<real_t>;
};

// ----------------------------------------------------------------------------
// Hermite-Corrected Trapezoidal Rule (uses endpoint derivatives)
// ----------------------------------------------------------------------------

/*!
  O(h^4) Hermite-corrected trapezoidal rule from pre-computed samples.

  Uses Euler-Maclaurin correction:
    Int[a,b] f(x)dx ~ (h/2)(f(a) + f(b)) - (h^2/12)(f'(b) - f'(a))

  \param a Left endpoint.
  \param b Right endpoint.
  \param samples Jets at {a, b}.
  \return Approximate integral with O(h^4) accuracy.
*/
inline auto trapezoid4(real_t a, real_t b,
                       const Trapezoid4Samples& samples) noexcept -> real_t {
  constexpr auto c0 = 0.5L;
  constexpr auto c1 = 1.0L / 12.0L;
  const auto h = b - a;
  const auto& [ja, jb] = samples;
  return (c0 * (ja.f + jb.f) + c1 * (ja.df - jb.df) * h) * h;
}

/*!
  O(h^4) Hermite-corrected trapezoidal rule with function evaluation.

  \param f Callable returning Jet at each point.
  \param a Left endpoint.
  \param b Right endpoint.
  \return Approximate integral with O(h^4) accuracy.
*/
template <JetCallable F>
auto trapezoid4(F&& f, real_t a, real_t b) noexcept -> real_t {
  return trapezoid4(a, b, Trapezoid4Samples{f(a), f(b)});
}

/*!
  O(h^8) Hermite-corrected trapezoidal rule from pre-computed samples.

  Uses two levels of Richardson extrapolation on grids at h, h/2, h/4.

  \param a Left endpoint.
  \param b Right endpoint.
  \param samples Jets at {a, a + h/4, a + h/2, a + 3h/4, b}.
  \return Approximate integral with O(h^8) accuracy.
*/
inline auto trapezoid8(real_t a, real_t b,
                       const Trapezoid8Samples& samples) noexcept -> real_t {
  const auto h = b - a;

  // Full step: I_h
  const auto I_h = trapezoid4(a, b, {samples[0], samples[4]});

  // Half steps: I_{h/2}
  const auto mid = 0.5L * (a + b);
  const auto I_h2 = trapezoid4(a, mid, {samples[0], samples[2]}) +
                    trapezoid4(mid, b, {samples[2], samples[4]});

  // Quarter steps: I_{h/4}
  const auto q1 = a + 0.25L * h;
  const auto q3 = a + 0.75L * h;
  const auto I_h4 = trapezoid4(a, q1, {samples[0], samples[1]}) +
                    trapezoid4(q1, mid, {samples[1], samples[2]}) +
                    trapezoid4(mid, q3, {samples[2], samples[3]}) +
                    trapezoid4(q3, b, {samples[3], samples[4]});

  // First Richardson: O(h^4) -> O(h^6)
  constexpr auto c_r1 = 1.0L / 15.0L;
  const auto R1_h = c_r1 * (16.0L * I_h2 - I_h);
  const auto R1_h2 = c_r1 * (16.0L * I_h4 - I_h2);

  // Second Richardson: O(h^6) -> O(h^8)
  constexpr auto c_r2 = 1.0L / 63.0L;
  return c_r2 * (64.0L * R1_h2 - R1_h);
}

/*!
  O(h^8) Hermite-corrected trapezoidal rule with function evaluation.

  Evaluates f at 5 points: endpoints and 3 interior quarter-points.

  \param f Callable returning Jet at each point.
  \param a Left endpoint.
  \param b Right endpoint.
  \return Approximate integral with O(h^8) accuracy.
*/
template <JetCallable F>
auto trapezoid8(F&& f, real_t a, real_t b) noexcept -> real_t {
  const auto h = b - a;
  const auto q1 = a + 0.25L * h;
  const auto mid = a + 0.5L * h;
  const auto q3 = a + 0.75L * h;

  return trapezoid8(a, b, Trapezoid8Samples{f(a), f(q1), f(mid), f(q3), f(b)});
}

// ----------------------------------------------------------------------------
// Gauss-Legendre Quadrature (does not use derivatives)
// ----------------------------------------------------------------------------

// Node locations for Gauss-Legendre on [-1, 1].
namespace gauss_nodes {

// 3-point: +/- sqrt(3/5), 0
constexpr auto g3_outer = 0.7745966692414833770L;

// 4-point
constexpr auto g4_outer = 0.8611363115940525752L;
constexpr auto g4_inner = 0.3399810435848562648L;

// 5-point
constexpr auto g5_outer = 0.9061798459386639928L;
constexpr auto g5_inner = 0.5384693101056830910L;

}  // namespace gauss_nodes

/*!
  Compute 3-point Gauss-Legendre sample locations for interval [a, b].

  Nodes are at: mid +/- sqrt(3/5) * half, mid
  Order in array: [left, center, right]
*/
inline auto gauss3_nodes(real_t a, real_t b) noexcept -> std::array<real_t, 3> {
  const auto mid = 0.5L * (a + b);
  const auto half = 0.5L * (b - a);
  return {mid - gauss_nodes::g3_outer * half, mid,
          mid + gauss_nodes::g3_outer * half};
}

/*!
  3-point Gauss-Legendre from pre-computed samples.

  Exact for polynomials up to degree 5, O(h^6) error for smooth functions.

  \param a Left endpoint.
  \param b Right endpoint.
  \param samples Function values at Gauss nodes (use gauss3_nodes to get
  locations).
  \return Approximate integral.
*/
inline auto gauss3(real_t a, real_t b, const Gauss3Samples& samples) noexcept
    -> real_t {
  constexpr auto w_outer = 5.0L / 9.0L;
  constexpr auto w_center = 8.0L / 9.0L;

  const auto half = 0.5L * (b - a);
  return half *
         (w_outer * samples[0] + w_center * samples[1] + w_outer * samples[2]);
}

/*!
  3-point Gauss-Legendre with function evaluation.

  \param f Callable returning function value at a point.
  \param a Left endpoint.
  \param b Right endpoint.
  \return Approximate integral.
*/
template <ValueCallable F>
auto gauss3(F&& f, real_t a, real_t b) noexcept -> real_t {
  const auto nodes = gauss3_nodes(a, b);
  return gauss3(a, b, Gauss3Samples{f(nodes[0]), f(nodes[1]), f(nodes[2])});
}

/*!
  Compute 4-point Gauss-Legendre sample locations for interval [a, b].
*/
inline auto gauss4_nodes(real_t a, real_t b) noexcept -> std::array<real_t, 4> {
  const auto mid = 0.5L * (a + b);
  const auto half = 0.5L * (b - a);
  return {
      mid - gauss_nodes::g4_outer * half, mid - gauss_nodes::g4_inner * half,
      mid + gauss_nodes::g4_inner * half, mid + gauss_nodes::g4_outer * half};
}

/*!
  4-point Gauss-Legendre from pre-computed samples.

  Exact for polynomials up to degree 7, O(h^8) error for smooth functions.
*/
inline auto gauss4(real_t a, real_t b, const Gauss4Samples& samples) noexcept
    -> real_t {
  constexpr auto w_outer = 0.3478548451374538574L;
  constexpr auto w_inner = 0.6521451548625461426L;

  const auto half = 0.5L * (b - a);
  return half * (w_outer * (samples[0] + samples[3]) +
                 w_inner * (samples[1] + samples[2]));
}

/*!
  4-point Gauss-Legendre with function evaluation.
*/
template <ValueCallable F>
auto gauss4(F&& f, real_t a, real_t b) noexcept -> real_t {
  const auto nodes = gauss4_nodes(a, b);
  return gauss4(
      a, b, Gauss4Samples{f(nodes[0]), f(nodes[1]), f(nodes[2]), f(nodes[3])});
}

/*!
  Compute 5-point Gauss-Legendre sample locations for interval [a, b].
*/
inline auto gauss5_nodes(real_t a, real_t b) noexcept -> std::array<real_t, 5> {
  const auto mid = 0.5L * (a + b);
  const auto half = 0.5L * (b - a);
  return {mid - gauss_nodes::g5_outer * half,
          mid - gauss_nodes::g5_inner * half, mid,
          mid + gauss_nodes::g5_inner * half,
          mid + gauss_nodes::g5_outer * half};
}

/*!
  5-point Gauss-Legendre from pre-computed samples.

  Exact for polynomials up to degree 9, O(h^10) error for smooth functions.
*/
inline auto gauss5(real_t a, real_t b, const Gauss5Samples& samples) noexcept
    -> real_t {
  constexpr auto w_outer = 0.2369268850561890875L;
  constexpr auto w_inner = 0.4786286704993664680L;
  constexpr auto w_center = 0.5688888888888888889L;

  const auto half = 0.5L * (b - a);
  return half * (w_outer * (samples[0] + samples[4]) +
                 w_inner * (samples[1] + samples[3]) + w_center * samples[2]);
}

/*!
  5-point Gauss-Legendre with function evaluation.
*/
template <ValueCallable F>
auto gauss5(F&& f, real_t a, real_t b) noexcept -> real_t {
  const auto nodes = gauss5_nodes(a, b);
  return gauss5(a, b,
                Gauss5Samples{f(nodes[0]), f(nodes[1]), f(nodes[2]),
                              f(nodes[3]), f(nodes[4])});
}

}  // namespace curves
