// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point wrapper for kernel spline data.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/math/spline.hpp>
#include <algorithm>
#include <cassert>

namespace curves {

struct SplineResult {
  real_t T;    // T(x)
  real_t dT;   // T'(x)
  real_t d2T;  // T''(x)
};

/*
  SplineView

  Non-owning view of a kernel spline with floating-point evaluation.
*/
class SplineView {
 public:
  // --------------------------------------------------------------------------
  // Construction
  // --------------------------------------------------------------------------

  /*
    Construct a view of an existing kernel spline.

    The spline pointer must remain valid for the lifetime of this view.
    Passing nullptr creates an empty view where all evaluations return 0.
  */
  explicit SplineView(const curves_spline* spline) : spline_{spline} {}

  /* Default constructor creates an empty view. */
  SplineView() = default;

  // --------------------------------------------------------------------------
  // Domain Information
  // --------------------------------------------------------------------------

  // Conversion factor from velocity to the reference domain of the spline.
  auto v_to_x() const noexcept -> real_t {
    return valid() ? fixed_to_real(spline_->v_to_x) : 0.0;
  }

  // Conversion factor from reference domain of the spline to velocity;
  auto x_to_v() const noexcept -> real_t {
    return valid() ? 1.0 / v_to_x() : 0.0;
  }

  // End of mapped spline domain. Beyond this is a linear extension.
  auto x_max() const noexcept -> real_t { return SPLINE_X_END_MAX; }

  // End of mapped spline domain in velocity.
  auto v_max() const noexcept -> real_t { return x_max() * x_to_v(); }

  // Checks if this view points to valid spline data.
  auto valid() const noexcept -> bool { return spline_ != nullptr; }

  // --------------------------------------------------------------------------
  // Evaluation from x-space (Spline Domain)
  // --------------------------------------------------------------------------

  // Evaluate T(x), T'(x), T''(x) at position x in spline's reference domain.
  auto operator()(real_t x) const -> SplineResult {
    if (!valid()) return {0.0, 0.0, 0.0};

    x = std::clamp(x, 0.0, static_cast<real_t>(SPLINE_X_END_MAX));
    const auto [seg, x_width, t] = resolve_segment(x);
    const auto x_width_inv = 1.0 / x_width;

    const auto c0 = fixed_to_real(seg->coeffs[0]);
    const auto c1 = fixed_to_real(seg->coeffs[1]);
    const auto c2 = fixed_to_real(seg->coeffs[2]);
    const auto c3 = fixed_to_real(seg->coeffs[3]);

    // Powers of t.
    const auto t2 = t * t;
    const auto t3 = t2 * t;

    // This should use the kernel c eval, but that is currently in v.
    auto T = c0 * t3 + c1 * t2 + c2 * t + c3;

    // dT/dt = 3*c0*t^2 + 2*c1*t + c2
    auto dT_dt = 3 * c0 * t2 + 2 * c1 * t + c2;
    auto dT = dT_dt * x_width_inv;

    // d^2T/dt^2 = 6*c0*t + 2*c1
    auto d2T_dt2 = 6 * c0 * t + 2 * c1;
    auto d2T = d2T_dt2 * x_width_inv * x_width_inv;

    return {T, dT, d2T};
  }

  // Evaluate T(x) only.
  auto eval(real_t x) const -> real_t {
    if (!valid()) return 0.0;

    x = std::clamp(x, 0.0, static_cast<real_t>(SPLINE_X_END_MAX));
    const auto [seg, x_width, t] = resolve_segment(x);

    auto c0 = fixed_to_real(seg->coeffs[0]);
    auto c1 = fixed_to_real(seg->coeffs[1]);
    auto c2 = fixed_to_real(seg->coeffs[2]);
    auto c3 = fixed_to_real(seg->coeffs[3]);

    auto t2 = t * t;
    auto t3 = t2 * t;

    return c0 * t3 + c1 * t2 + c2 * t + c3;
  }

  // --------------------------------------------------------------------------
  // Evaluation at u (shaped velocity space)
  // --------------------------------------------------------------------------

  auto at_u(real_t u) const -> SplineResult {
    auto x = u * v_to_x();
    return (*this)(x);
  }

  auto eval_at_u(real_t u) const -> real_t {
    auto x = u * v_to_x();
    return eval(x);
  }

 private:
  const curves_spline* spline_ = nullptr;

  struct ResolvedSegment {
    const curves_spline_segment* segment;
    real_t width;
    real_t t;
  };

  /*
    Find the segment containing x and compute local parameter t.

    Returns {segment_pointer, t} where t in [0, 1] is the position
    within the segment.
  */
  auto resolve_segment(real_t x) const noexcept -> ResolvedSegment {
    assert(valid());

    const auto x_fixed = real_to_fixed(x);
    const auto segment_desc = spline::calc_segment_desc(x_fixed);
    const auto index = std::min(segment_desc.index, SPLINE_NUM_SEGMENTS);
    const auto t = std::min(1.0, fixed_to_real(spline::map_x_to_t(
                                      x_fixed, segment_desc.width_log2)));
    const auto width = fixed_to_real(1LL << segment_desc.width_log2);
    return ResolvedSegment{
        .segment = &spline_->segments[index],
        .width = width,
        .t = t,
    };
  }

  /*
    Evaluate cubic polynomial at local parameter t using Horner's method.

    T(t) = c0*t^3 + c1*t^2 + c2*t + c3
         = ((c0*t + c1)*t + c2)*t + c3
  */
  auto evaluate_cubic(const curves_spline_segment& segment,
                      real_t t) const noexcept -> real_t {
    const auto* c = segment.coeffs;
    const auto c0 = fixed_to_real(c[0]);
    const auto c1 = fixed_to_real(c[1]);
    const auto c2 = fixed_to_real(c[2]);
    const auto c3 = fixed_to_real(c[3]);

    return ((c0 * t + c1) * t + c2) * t + c3;
  }
};

}  // namespace curves
