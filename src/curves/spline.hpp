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
The limit at 0 comes from L'Hopital, since v(0) = 0 and v'(x) = f(x):
  sensitivity(0) = lim_{x->0} v(x)/x = f(0)

For sensitivity'(0), if you Taylor expand:
  v(x) = f(0)x + f'(0)x^2/2 + O(x^3)
then:
  sensitivity(x) = v(x)/x = f(0) + f'(0)x/2 + O(x^2)
*/
template <typename Sensitivity>
auto generate_table_from_sensitivity(Sensitivity s, real_t x_max)
    -> curves_spline_table {
  real_t x_scale = CURVES_SPLINE_NUM_SEGMENTS / x_max;
  curves_spline_table result;
  result.x_max = to_fixed(x_max, CURVES_SPLINE_FRAC_BITS);
  result.x_scale = to_fixed(x_scale, CURVES_SPLINE_FRAC_BITS);

  const auto dx = x_max / CURVES_SPLINE_NUM_SEGMENTS;
  auto x = 0.0L;

  for (auto entry = 0; entry < CURVES_SPLINE_NUM_SEGMENTS; ++entry) {
    auto [y0, m0] = s(x);
    m0 *= dx;  // Tangent is derivative scaled to interval width.

    auto [y1, m1] = s(x + dx);
    m1 *= dx;

    // Standard Hermite Poly Construction.
    auto& fixed_coeffs = result.coeffs[entry];
    const real_t float_coeffs[] = {y0, m0, 3 * (y1 - y0) - 2 * m0 - m1,
                                   2 * (y0 - y1) + m0 + m1};

    for (auto coeff = 0; coeff < CURVES_SPLINE_NUM_COEFFS; ++coeff) {
      fixed_coeffs[coeff] =
          to_fixed(float_coeffs[coeff], CURVES_SPLINE_FRAC_BITS);
    }

    x += dx;
  }

  // Boundary conditions; calc the final point's value and slope explicitly
  const auto [y_max, m_max] = s(x);
  result.y_max = to_fixed(y_max, CURVES_SPLINE_FRAC_BITS);
  result.m_max = to_fixed(m_max, CURVES_SPLINE_FRAC_BITS);

  return result;
}

template <typename Gain>
auto generate_table_from_gain(Gain g, real_t x_max) -> curves_spline_table {
  curves_spline_table result;

  auto dx = x_max / CURVES_SPLINE_NUM_SEGMENTS;
  auto x = 0.0L;
  auto y = g(x);
  auto v = 0.0L;
  auto s = y;

  // The initial value of m comes from the limit using l'hopital. This is the
  // second order forward difference.
  auto m = (-g(2 * dx) + 4 * g(dx) - 3 * y) * dx / (2 * dx);

  int_t min_frac_bits[CURVES_SPLINE_NUM_COEFFS] = {64, 64, 64, 64};
  int_t max_frac_bits[CURVES_SPLINE_NUM_COEFFS] = {0, 0, 0, 0};

  for (auto entry = 0; entry < CURVES_SPLINE_NUM_SEGMENTS; ++entry) {
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
    real_t float_coeffs[] = {s0, m0, 3 * (s - s0) - 2 * m0 - m,
                             2 * (s0 - s) + m0 + m};
    for (auto coeff = 0; coeff < CURVES_SPLINE_NUM_COEFFS; ++coeff) {
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

  auto cusp() const noexcept -> std::optional<real_t> {
    return std::make_optional(p);
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

  auto cusp() const noexcept -> std::optional<real_t> { return curve_.cusp(); }

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

struct SplineKnots {
  std::vector<Knot> knots;

  auto find_nearest(real_t x) const noexcept -> int_t {
    if (knots.empty()) return -1;

    const auto begin = std::begin(knots);
    const auto end = std::end(knots);

    // Find where a node with value x would be inserted.
    auto insertion_location = std::lower_bound(
        begin, end, x,
        [](const auto& knot, const auto x) noexcept { return knot.x < x; });

    // Range-check result.
    if (insertion_location == begin) return 0;
    if (insertion_location == end) return std::ssize(knots) - 1;

    // Choose closer of nearby pair.
    const auto prev = std::prev(insertion_location);
    return std::abs(prev->x - x) < std::abs(insertion_location->x - x)
               ? std::distance(begin, prev)
               : std::distance(begin, insertion_location);
  }

  /*
    Applies Catmull-Rom to a node's tangent to fix a cusp where the analytical
    solution breaks.
  */
  auto remove_analytical_cusp(int_t i) noexcept -> void {
    assert(std::ssize(knots) >= 3 && i > 0 && i < std::ssize(knots) - 1);
    const auto dx = knots[i + 1].x - knots[i - 1].x;
    const auto dy = knots[i + 1].y - knots[i - 1].y;
    knots[i].m = dy / dx;
  }

  static auto create(const auto& curve, real_t dx, int_t num_knots)
      -> SplineKnots {
    if (!num_knots) return SplineKnots{};

    auto knots = std::vector<Knot>{};
    knots.reserve(num_knots);

    for (auto i = 0; i < num_knots; ++i) {
      const auto x = i * dx;
      auto result = curve(x);
      knots.emplace_back(x, result.f, result.df_dx);
    }

    auto result = SplineKnots{std::move(knots)};

#if 0
    auto cusp = curve.cusp();
    if (!cusp) return result;

    auto nearest = result.find_nearest(*cusp);
    if (nearest <= 0 || nearest >= std::ssize(result.knots) - 1) return result;

    result.remove_analytical_cusp(nearest);
#endif

    return result;
  }
};

// Ideal x_max is 128, but we let it float a little so curves with cusps can
// align a knot to the cusp.
constexpr int_t x_max_ideal = 128;

struct SegmentLayout {
  real_t segment_width;
  real_t x_max;
};

inline auto create_segment_layout(real_t /*crossover*/) noexcept
    -> SegmentLayout {
#if 1
  return {0.5, 128};
#else
  assert(crossover > 0);
  const auto ideal_cusp_index = static_cast<int_t>(
      std::round(crossover * CURVES_SPLINE_NUM_SEGMENTS / x_max_ideal));
  const auto clamped_ideal_cusp_index =
      std::clamp<int_t>(ideal_cusp_index, 1, CURVES_SPLINE_NUM_SEGMENTS - 1);

  const auto segment_width = crossover / clamped_ideal_cusp_index;
  const auto x_max = segment_width * CURVES_SPLINE_NUM_SEGMENTS;
  return {segment_width, x_max};
#endif
}

inline auto create_spline(const auto& curve, real_t crossover) noexcept
    -> curves_spline {
  assert(crossover > 0);

  curves_spline result;

  const auto segment_layout = create_segment_layout(crossover);
  result.inv_segment_width = Fixed(1.0 / segment_layout.segment_width).value;
  result.x_max = Fixed(segment_layout.x_max).value;

  const auto spline_knots = SplineKnots::create(TransferAdapterCurve{curve},
                                                segment_layout.segment_width,
                                                CURVES_SPLINE_NUM_SEGMENTS + 1);

  auto* p0 = spline_knots.knots.data();
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

  return result;
}

static inline s64 curves_spline_eval(const struct curves_spline* spline,
                                     s64 x) {
  // Validate parameters.
  if (unlikely(x < 0)) x = 0;
  if (x >= spline->x_max) {
    // Extend final tangent.

    // Extract named cubic coefs.
    const s64* coeff = spline->coeffs[CURVES_SPLINE_NUM_SEGMENTS - 1];
    s64 a = *coeff++;
    s64 b = *coeff++;
    s64 c = *coeff++;
    s64 d = *coeff++;

    // Calc slope and y at x_max.
    s64 m = (s64)(((s128)(3 * a + 2 * b + c) * spline->inv_segment_width) >>
                  CURVES_SPLINE_FRAC_BITS);
    s64 y_max = a + b + c + d;

    // y = m(x - x_max) + y_max
    return (s64)(((s128)m * (x - spline->x_max)) >> CURVES_SPLINE_FRAC_BITS) +
           y_max;
  }

  // Index into segment with normalized t.
  s64 x_segment =
      (s64)(((s128)x * spline->inv_segment_width) >> CURVES_SPLINE_FRAC_BITS);
  s64 segment_index = x_segment >> CURVES_SPLINE_FRAC_BITS;
  s64 t = x_segment & ((1LL << CURVES_SPLINE_FRAC_BITS) - 1);

  // Horner's loop.
  const s64* coeff = spline->coeffs[segment_index];
  s64 result = *coeff++;
  for (int i = 1; i < CURVES_SPLINE_NUM_COEFFS; ++i) {
    result = (s64)(((s128)result * t) >> CURVES_SPLINE_FRAC_BITS);
    result += *coeff++;
  }

  return result;
}

struct Node {
  real_t x;
  real_t y;  // Transfer function result, velocity
  real_t m;  // Slope of transfer function, gain
};

struct CubicSegment {
  s64 a;
  s64 b;
  s64 c;
  s64 d;
};

/**
 * \brief Converts floating point coefficients to fixed-point kernel format.
 *
 * evaluates: y = ((a*t + b)*t + c)*t + d
 * Where 't' is normalized to [0, 1.0) in Q32.32
 */
inline CubicSegment pack_cubic(real_t a, real_t b, real_t c, real_t d) {
  const auto scale = static_cast<real_t>(1LL << CURVES_SPLINE_FRAC_BITS);
  return CubicSegment{
      static_cast<int64_t>(a * scale), static_cast<int64_t>(b * scale),
      static_cast<int64_t>(c * scale), static_cast<int64_t>(d * scale)};
}

/**
 * \brief Solves the Hermite cubic coefficients for interval [0, 1].
 *
 * Given start/end points and tangents, returns coefficients for:
 * f(t) = at^3 + bt^2 + ct + d
 */
inline CubicSegment solve_hermite_unit(const Node& p0, const Node& p1) {
  real_t dx = p1.x - p0.x;

  // Normalize tangents to the unit interval t in [0, 1]
  // m_unit = m_original * dx
  real_t m0 = p0.m * dx;
  real_t m1 = p1.m * dx;
  real_t y0 = p0.y;
  real_t y1 = p1.y;

  // Standard Hermite Basis (Unit Domain)
  real_t a = 2 * y0 - 2 * y1 + m0 + m1;
  real_t b = -3 * y0 + 3 * y1 - 2 * m0 - m1;
  real_t c = m0;
  real_t d = y0;

  return pack_cubic(a, b, c, d);
}

struct SplineSet {
  uint64_t inv_p_scalar;  // Multiplier to normalize input relative to p
  int32_t min_k;          // fls of bucket 0
  int32_t max_k;          // fls of bucket n
  int32_t subdiv_bits;  // binary exponent of number of subdivisions per octave
  std::vector<CubicSegment> segments;
};

// Samples the Transfer Function
auto sample_node(real_t x, auto curve) -> Node {
  if (x <= 0) return {0.0, 0.0, 0.0};  // At 0, T=0, G=finite (handled below)

  auto val = curve(x);  // returns { sensitivity, d_sensitivity }

  // T(x) = x * S(x)
  real_t y = x * val.f;

  // G(x) = S(x) + x * S'(x)
  real_t m = val.f + x * val.df_dx;

  return {x, y, m};
};

#define use_analytical_tangents

/*
  Technically, this is a piecewise cubic Hermite interpolant (PCHIP) on a
  geometrically refined mesh, using centered difference tangents and quadratic
  end conditions. You could describe it as a custom Catmull-Rom LUT with
  geometric LOD.
*/

/**
 * \brief Generates a P-Centered Geometric Spline Set for the kernel.
 * * \param curve         Functor returning {f, df_dx} (Sensitivity, not
 * Transfer)
 * \param sync_speed    The 'p' parameter around which the grid is centered
 * \param max_speed     The maximum input speed to cover
 * \param min_resolution_bits  Number of bits below 'p' to simulate
 * geometrically (e.g. 6)
 * \param subdiv_bits Binary exponent of number of subdivisions per octave.
 */
template <typename CurveT>
SplineSet generate_transfer_splines(const CurveT& curve, real_t sync_speed,
                                    real_t max_speed,
                                    int min_resolution_bits = 1,
                                    int subdiv_bits = 6) {
  SplineSet result;

  // 1. Setup
  result.inv_p_scalar = static_cast<uint64_t>(std::pow(2.0, 32.0) / sync_speed);
  result.subdiv_bits = subdiv_bits;
  result.min_k = (32 - min_resolution_bits) + 1;

  real_t p = sync_speed;
  real_t inv_p_scalar = std::pow(2.0, 32.0) / p;

  // Calc max_k...
  real_t max_norm = max_speed * inv_p_scalar;
  int max_fls = 0;
  if (max_norm > 0)
    max_fls = static_cast<int>(std::floor(std::log2(max_norm))) + 1;
  result.max_k = std::max(max_fls, result.min_k + 1);

  // 2. Buffer ALL points first
  // We need a continuous list of (x, y) to calculate neighbor-based tangents
  std::vector<Node> nodes;

  // A. Start Point (0,0)
  // Note: We ignore the analytical derivative gain_limit here.
  // We will calculate the slope from 0 to the first point numerically.
  nodes.push_back({0.0, 0.0, 0.0});

  // B. Zero Bucket Points
  int sub_steps = 1 << subdiv_bits;
  real_t zero_bucket_width = std::pow(2.0, result.min_k);

  for (int i = 0; i < sub_steps; ++i) {
    real_t t_end = (real_t)(i + 1) / sub_steps;
    real_t norm_x = zero_bucket_width * t_end;

    // Only calculate x and y. Ignore m for now.
    real_t x = norm_x / inv_p_scalar;

#if defined use_analytical_tangents
    nodes.push_back(sample_node(x, curve));
#else
    // y = S(x)
    auto val = curve(x);

    // T(x) = x * S(x)
    nodes.push_back({x, x * val.f, 0.0});
#endif
  }

  // C. Geometric Bucket Points
  for (int k = result.min_k + 1; k <= result.max_k; ++k) {
    real_t norm_start = std::pow(2.0, k - 1);
    real_t width = norm_start;

    for (int i = 0; i < sub_steps; ++i) {
      real_t sub_t = (real_t)(i + 1) / sub_steps;
      real_t norm_x = norm_start + (width * sub_t);

      real_t x = norm_x / inv_p_scalar;
#if defined use_analytical_tangents
      nodes.push_back(sample_node(x, curve));
#else
      auto val = curve(x);
      nodes.push_back({x, x * val.f, 0.0});
#endif
    }
  }

#if !defined use_analytical_tangents
  // 3. Compute Numerical Tangents
  // (Catmull-Rom / Central Difference, Robust 2nd Order)
  for (size_t i = 0; i < nodes.size(); ++i) {
    real_t m;

    // Check if we have enough points for 3-point stencils
    if (nodes.size() < 3) {
      // Fallback for extremely low resolution (unlikely)
      if (i < nodes.size() - 1)
        m = (nodes[i + 1].y - nodes[i].y) / (nodes[i + 1].x - nodes[i].x);
      else
        m = (nodes[i].y - nodes[i - 1].y) / (nodes[i].x - nodes[i - 1].x);
    } else if (i == 0) {
      // Start: 3-Point Forward Difference
      // Assumes uniform spacing h between x0, x1, x2 (guaranteed by zero
      // bucket) f'(x0) ≈ (-3y0 + 4y1 - y2) / 2h
      real_t h = nodes[1].x - nodes[0].x;
      m = (-3.0 * nodes[0].y + 4.0 * nodes[1].y - nodes[2].y) / (2.0 * h);
    } else if (i == nodes.size() - 1) {
      // End: 3-Point Backward Difference
      // Assumes uniform spacing h between x(n-2), x(n-1), x(n)
      // f'(xn) ≈ (3yn - 4y(n-1) + y(n-2)) / 2h
      real_t h = nodes[i].x - nodes[i - 1].x;
      m = (3.0 * nodes[i].y - 4.0 * nodes[i - 1].y + nodes[i - 2].y) /
          (2.0 * h);
    } else {
      // Interior: Central Difference
      // Even if adjacent intervals differ slightly in size (at geometric
      // boundaries), the weighted central difference is safer. However, since
      // we are doing uniform subdivisions, simple central is usually fine.
      // Standard Central: (y_next - y_prev) / (x_next - x_prev)
      m = (nodes[i + 1].y - nodes[i - 1].y) / (nodes[i + 1].x - nodes[i - 1].x);
    }

    nodes[i].m = m;
  }
#endif

  // 4. Generate Segments
  for (size_t i = 0; i < nodes.size() - 1; ++i) {
    result.segments.push_back(solve_hermite_unit(nodes[i], nodes[i + 1]));
  }

  return result;
}

static inline s64 q32_mul(s64 x, s64 y) { return (s64)(((s128)x * y) >> 32); }

/**
 * eval_transfer_curve - Evaluates the Transfer Function T(x)
 * @x_input: The input magnitude (usually counts per time).
 * @set:     The spline dataset.
 *
 * Returns the output magnitude (Q32.32) to be applied.
 * To get the scalar multiplier: result / x_input.
 */
static inline s64 eval_transfer_curve(const struct SplineSet* set,
                                      u64 x_input) {
  if (x_input == 0) return 0;

  /*
   * 1. Normalize
   * Map 'p' (the sync speed) to the 32nd bit (2^32).
   * Input is Q32.32. inv_p_scalar scales physical units to the grid.
   * We cast to u128 for the multiply, then shift back down to keep Q32.32.
   */
  u64 norm = (u64)(((u128)x_input * set->inv_p_scalar) >> 32);

  /*
   * 2. Find Magnitude Index (1-based MSB)
   * idx corresponds to the power of 2 bucket.
   */
  int idx = 64 - curves_clz64(norm);

  // 3. Calculate "Global t" for the octave/bucket
  u64 t_octave;

  // Base index into the segments vector (before subdivision offset)
  int segment_base_idx;

  /*
   * 3. Select Segment and Calculate 't'
   */

  // --- Case A: Linear Extension (Input > Max Speed) ---
  if (idx > set->max_k) {
    // Use the very last segment to calculate the tangent
    segment_base_idx = (set->max_k - set->min_k) << set->subdiv_bits;
    u32 sub_idx = (1LL << set->subdiv_bits) - 1;
    const struct CubicSegment* seg = &set->segments[segment_base_idx + sub_idx];

    // y(1) = a + b + c + d
    s64 y_end = seg->a + seg->b + seg->c + seg->d;

    // y'(1) = 3a + 2b + c
    s64 slope_end = 3 * seg->a + 2 * seg->b + seg->c;

    // Calculate delta: how far past the end of the last bucket are we?
    // Last bucket ends at 2^max_k
    u64 bucket_end = 1ULL << set->max_k;
    u64 delta_norm = norm - bucket_end;

    // Convert delta_norm to 't' units relative to the last bucket's width.
    // Width = 2^(max_k - 1).
    // dt = delta / width
    // In Q32.32: dt = (delta << 32) >> (max_k - 1) = delta << (33 - max_k)
    int shift = 33 - set->max_k;
    s64 dt;
    if (shift >= 0)
      dt = delta_norm << shift;
    else
      dt = delta_norm >> (-shift);

    // Result = y_end + slope * dt
    return y_end + q32_mul(slope_end, dt);
  }

  // --- Case B: Zero Bucket ---
  if (idx <= set->min_k) {
    segment_base_idx = 0;  // Starts at 0

    // Map [0, 2^min_k] -> [0, 1.0)
    int shift = 32 - set->min_k;
    if (shift >= 0)
      t_octave = norm << shift;
    else
      t_octave = norm >> (-shift);

    // --- Case C: Geometric Buckets ---
  } else {
    // Offset = (zero_bucket_subdivs) + (my_octave_offset * subdivs)

    segment_base_idx = (1 + (idx - set->min_k - 1)) << set->subdiv_bits;

    // Mask out the leading bit (the 'start' of the bucket)
    u64 remainder = norm & ~(1ULL << (idx - 1));

    // Map [0, width] -> [0, 1.0)
    int shift = 33 - idx;
    if (shift >= 0)
      t_octave = remainder << shift;
    else
      t_octave = remainder >> (-shift);
  }

  /*
   * 4. Subdivision Magic
   * t_octave is [0, 1.0) mapped to [0, 2^32).
   * We want to split this into 2^subdiv_bits parts.
   *
   * High bits -> Which sub-segment?
   * Low bits  -> New t for that segment.
   */

  // Extract top bits for index
  int shift_sub = 32 - set->subdiv_bits;

  // The sub-index within this octave
  u32 sub_idx = t_octave >> shift_sub;

  // The final segment index
  const struct CubicSegment* seg = &set->segments[segment_base_idx + sub_idx];

  // The new 't' is the remaining bits, shifted up to fill Q32.32
  u64 t = (t_octave << set->subdiv_bits) & 0xFFFFFFFFULL;

  /* 5. Horner's Method
   * y = ((a*t + b)*t + c)*t + d
   */
  s64 s_t = (s64)t;
  s64 res = seg->a;
  res = q32_mul(res, s_t);
  res += seg->b;
  res = q32_mul(res, s_t);
  res += seg->c;
  res = q32_mul(res, s_t);
  res += seg->d;

  return res;
}

}  // namespace curves
