// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel spline module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

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

using real_t = long double;

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
  real_t x_scale = CURVES_SPLINE_TABLE_SIZE / x_max;
  curves_spline_table result;
  result.x_max = to_fixed(x_max, CURVES_SPLINE_FRAC_BITS);
  result.x_scale = to_fixed(x_scale, CURVES_SPLINE_FRAC_BITS);

  const auto dx = x_max / CURVES_SPLINE_TABLE_SIZE;
  auto x = 0.0L;

  for (auto entry = 0; entry < CURVES_SPLINE_TABLE_SIZE; ++entry) {
    auto [y0, m0] = s(x);
    m0 *= dx;  // Tangent is derivative scaled to interval width.

    auto [y1, m1] = s(x + dx);
    m1 *= dx;

    // Standard Hermite Poly Construction.
    auto& fixed_coeffs = result.coeffs[entry];
    const real_t float_coeffs[] = {y0, m0, 3 * (y1 - y0) - 2 * m0 - m1,
                                   2 * (y0 - y1) + m0 + m1};

    for (auto coeff = 0; coeff < CURVES_SPLINE_NUM_COEFS; ++coeff) {
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
    real_t float_coeffs[] = {s0, m0, 3 * (s - s0) - 2 * m0 - m,
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

class SynchronousCurve {
 public:
  SynchronousCurve(real_t scale, real_t motivity, real_t gamma,
                   real_t sync_speed, real_t smooth)
      : s(scale),
        L(std::log(motivity)),
        g(gamma / L),
        p(sync_speed),
        k(smooth == 0 ? 16.0 : 0.5 / smooth),
        r(1.0 / k) {}

  struct Result {
    real_t f;      // function value
    real_t df_dx;  // derivative
  };

  Result operator()(real_t x) const {
    if (x == p) {
      return {s, 0.0};
    }

    if (x > p) {
      return evaluate<+1>(g * (std::log(x) - std::log(p)), x);
    } else {
      return evaluate<-1>(g * (std::log(p) - std::log(x)), x);
    }
  }

 private:
  real_t s;  // final output scale
  real_t L;  // log(motivity)
  real_t g;  // gamma / L
  real_t p;  // sync_speed
  real_t k;  // sharpness = 0.5 / smooth
  real_t r;  // 1 / sharpness

  // 'sign' is +1 for x > p, -1 for x < p.
  // It only affects the exponent of f; the derivative formula is invariant.
  template <int sign>
  Result evaluate(real_t u, real_t x) const {
    // Shared intermediate terms
    real_t u_pow_k_minus_1 = std::pow(u, k - 1);
    real_t u_pow_k = u_pow_k_minus_1 * u;  // v = u^k

    real_t w = std::tanh(u_pow_k);  // w = tanh(v)
    real_t w_pow_r_minus_1 = std::pow(w, r - 1);
    real_t w_pow_r = w_pow_r_minus_1 * w;  // z = w^r

    real_t sech_sq = 1 - w * w;  // sech(v)^2

    // Forward: f = exp((+/-)L * z)
    real_t f = s * std::exp(sign * L * w_pow_r);

    // Derivative: df/dx = (f * L * g / x) * u^(k-1) * w^(r-1) * sech(v)^2
    real_t df_dx =
        (f * L * g / x) * u_pow_k_minus_1 * w_pow_r_minus_1 * sech_sq;

    return {f, df_dx};
  }
};

// Simple container for Hermite node data
struct Node {
  real_t x;
  real_t y;  // Transfer function value (output velocity)
  real_t m;  // Gain (slope of transfer function)
};

// Kernel-compatible coefficient structure (Fixed Point Q32.32)
struct CubicSegment {
  int64_t a, b, c, d;
};

// The complete data blob for the kernel
struct SplineSet {
  uint64_t inv_p_scalar;  // Multiplier to normalize input relative to p
  int32_t min_k;  // The 'fls' index corresponding to the zero bucket limit
  int32_t max_k;  // The maximum supported 'fls' index
  int32_t subdiv_bits;  // power of 2 number of subdivisions per octave
  std::vector<CubicSegment> segments;
};

/**
 * \brief Converts floating point coefficients to fixed-point kernel format.
 *
 * The kernel evaluates: y = ((a*t + b)*t + c)*t + d
 * Where 't' is normalized to [0, 1.0) in Q32.32
 */
inline CubicSegment pack_cubic(real_t a, real_t b, real_t c, real_t d) {
  // Scaling factor for Q32.32 fixed point
  const real_t scale = 4294967296.0;
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
                                    int subdiv_bits = 9) {
  SplineSet result;

  // 1. Calculate Grid Scalar
  // We map 'sync_speed' to 2^32. This puts 'p' at the 32nd bit.
  // Normalized x = input * inv_p_scalar
  real_t p = sync_speed;
  real_t inv_p_scalar = std::pow(2.0, 32.0) / p;
  result.inv_p_scalar = static_cast<uint64_t>(inv_p_scalar);
  result.subdiv_bits = subdiv_bits;
  int sub_steps = 1 << subdiv_bits;

  // 2. Define Bounds
  result.min_k = (32 - min_resolution_bits) + 1;

  // Calculate max_k based on max_speed
  // max_norm = max_speed * scalar
  real_t max_norm = max_speed * inv_p_scalar;
  int max_fls = 0;
  if (max_norm > 0) {
    max_fls = static_cast<int>(std::floor(std::log2(max_norm))) + 1;
  }
  result.max_k = std::max(max_fls, result.min_k + 1);

  // Range [0, 2^min_k]
  real_t zero_bucket_width = std::pow(2.0, result.min_k);

  // We need the special start node at 0
  real_t eps = std::numeric_limits<real_t>::epsilon();
  auto val_lim = curve(eps);
  real_t gain_limit = val_lim.f;
  Node n_prev = {0.0, 0.0, gain_limit};

  for (int i = 0; i < sub_steps; ++i) {
    // Linear interpolation of the bucket range
    real_t t_end = (real_t)(i + 1) / sub_steps;
    real_t norm_end = zero_bucket_width * t_end;

    Node n_next = sample_node(norm_end / inv_p_scalar, curve);
    result.segments.push_back(solve_hermite_unit(n_prev, n_next));
    n_prev = n_next;
  }

  // 5. Generate Geometric Buckets
  // Iterate from min_k up to max_k
  // Each bucket 'k' covers [2^(k-1), 2^k] in normalized space
  for (int k = result.min_k + 1; k <= result.max_k; ++k) {
    real_t norm_start_base = std::pow(2.0, k - 1);
    real_t width = norm_start_base;  // Width of octave k is 2^(k-1)

    // Ensure continuity: Start where the previous bucket ended
    // (n_prev is already set from the previous loop iteration)

    for (int i = 0; i < sub_steps; ++i) {
      real_t sub_t_end = (real_t)(i + 1) / sub_steps;
      real_t norm_current = norm_start_base + (width * sub_t_end);

      Node n_next = sample_node(norm_current / inv_p_scalar, curve);
      result.segments.push_back(solve_hermite_unit(n_prev, n_next));
      n_prev = n_next;
    }
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

#if 0
  std::cout << "segment_base_idx: " << segment_base_idx
            << " sub_idx: " << sub_idx << " t: " << t
            << " t_octave: " << t_octave << std::endl;
#endif

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
