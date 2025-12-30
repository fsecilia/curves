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

/*
  SplineView

  Non-owning view of a kernel spline with floating-point evaluation.

  Calls the actual kernel spline code. Uses fixed-point conversion to ensure
  the UI shows exactly what the driver calculates. It provides a float
  interface for:
    - Evaluating the transfer function, T(x), and its derivatives.
    - Evaluating display curves for sensitivity, S(x), and gain, G(x).
    - Domain queries (x_max, v_max).
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
  explicit SplineView(const curves_spline* spline)
      : spline_{spline},
        v_to_x_{spline ? fixed_to_real(spline->v_to_x) : 0.0L},
        x_to_v_{std::abs(v_to_x_) > 0.0L ? 1.0L / v_to_x_ : 0.0L} {}

  /* Default constructor creates an empty view. */
  SplineView() = default;

  // --------------------------------------------------------------------------
  // Domain Information
  // --------------------------------------------------------------------------

  // Conversion factor from velocity to the reference domain of the spline.
  auto v_to_x() const noexcept -> real_t { return v_to_x_; }

  // Conversion factor from reference domain of the spline to velocity;
  auto x_to_v() const noexcept -> real_t { return x_to_v_; }

  // End of mapped spline domain. Beyond this is a linear extension.
  auto x_max() const noexcept -> real_t {
    return fixed_to_real(SPLINE_X_END_MAX);
  }

  // End of mapped spline domain in velocity.
  auto v_max() const noexcept -> real_t { return x_max() * x_to_v_; }

  // Checks if this view points to valid spline data.
  auto valid() const noexcept -> bool { return spline_ != nullptr; }

  // --------------------------------------------------------------------------
  // Evaluation
  // --------------------------------------------------------------------------

  /*
    Evaluate T(v) where v is velocity
  */
  auto operator()(real_t v) const noexcept -> real_t {
    if (!valid()) return 0.0;
    const auto x = v * v_to_x_;
    return at_x(x);
  }

  // --------------------------------------------------------------------------
  // Evaluation from X-Space (Spline Domain)
  // --------------------------------------------------------------------------

  /*
    Evaluate T(x) directly in spline domain.

    Use this when working directly with spline coordinates, such as
    when rendering the curve or computing knot positions.
  */
  auto at_x(real_t x) const noexcept -> real_t {
    if (!valid()) return 0.0L;

    x = std::clamp(x, 0.0L, x_max());
    const auto resolved_segment = resolve_segment(x);
    return evaluate_cubic(*resolved_segment.segment, resolved_segment.t);
  }

  /*
    First derivative dT/dx at position x.

    For a cubic T(t) = c0*t^3 + c1*t^2 + c2*t + c3, the derivative is:
      dT/dt = 3*c0*t^2 + 2*c1*t + c2

    Since t = (x - x_begin) / x_width, we have dt/dx = 1/x_width, so:
      dT/dx = (dT/dt) / x_width
  */
  auto derivative_at_x(real_t x) const noexcept -> real_t {
    if (!valid()) return 0.0L;

    x = std::clamp(x, 0.0L, x_max());
    const auto resolved_segment = resolve_segment(x);

    const auto* c = resolved_segment.segment->coeffs;
    const auto c0 = fixed_to_real(c[0]);
    const auto c1 = fixed_to_real(c[1]);
    const auto c2 = fixed_to_real(c[2]);
    const auto t = resolved_segment.t;

    // dT/dt via Horner:
    auto dT_dt = (3 * c0 * t + 2 * c1) * t + c2;

    const auto x_width = fixed_to_real(resolved_segment.width);
    return (x_width > 0) ? (dT_dt / x_width) : 0.0;
  }

  /*
    Second derivative d^2T/dx^2 at position x.

    For the cubic:
      d^2T/dt^2 = 6*c0*t + 2*c1
      d^2T/dx^2 = (d^2T/dt^2) / x_width^2

    Useful for curvature analysis or finding inflection points.
  */
  auto second_derivative_at_x(real_t x) const noexcept -> real_t {
    if (!valid()) return 0.0L;

    x = std::clamp(x, 0.0L, x_max());
    auto resolved_segment = resolve_segment(x);

    const auto* c = resolved_segment.segment->coeffs;
    const auto c0 = fixed_to_real(c[0]);
    const auto c1 = fixed_to_real(c[1]);
    const auto t = resolved_segment.t;

    const auto d2T_dt2 = 6 * c0 * t + 2 * c1;

    const auto x_width = fixed_to_real(resolved_segment.width);
    return (x_width > 0) ? (d2T_dt2 / (x_width * x_width)) : 0.0;
  }

  // --------------------------------------------------------------------------
  // Display Curve Evaluation
  // --------------------------------------------------------------------------

  /*
    Sensitivity curve S(x) = T(x) / x.

    This is what the user sees in "sensitivity" mode. Near x=0, we use
    L'Hopital's rule to handle the 0/0 form: lim[x->0] T(x)/x = T'(0).
  */
  auto sensitivity_at_x(real_t x) const noexcept -> real_t {
    if (!valid()) return 0.0;

    constexpr const auto kEpsilon = 1e-10L;
    if (x < kEpsilon) {
      /* At origin, S(x) -> T'(0). */
      return derivative_at_x(0.0);
    }

    return at_x(x) / x;
  }

  /*
    Sensitivity derivative S'(x).

    From S(x) = T(x)/x:
      S'(x) = (T'(x)*x - T(x)) / x^2
            = (T'(x) - T(x)/x) / x
            = (T'(x) - S(x)) / x

    Near x=0, we use the Taylor expansion limit:
      S(x) ~= T'(0) + T''(0)*x/2 + ...
      S'(x) ~= T''(0)/2 + ...
  */
  auto sensitivity_derivative_at_x(real_t x) const noexcept -> real_t {
    if (!valid()) return 0.0;

    constexpr const auto kEpsilon = 1e-10L;
    if (x < kEpsilon) {
      return second_derivative_at_x(0.0) / 2.0;
    }

    const auto T_val = at_x(x);
    const auto T_prime = derivative_at_x(x);
    const auto S_val = T_val / x;

    return (T_prime - S_val) / x;
  }

  /*
    Gain curve G(x) = T'(x).

    This is just the first derivative, named for clarity when the
    user is in "gain" mode.

   */
  auto gain_at_x(real_t x) const noexcept -> real_t {
    return derivative_at_x(x);
  }

  /*
    Gain derivative G'(x) = T''(x).
  */
  auto gain_derivative_at_x(real_t x) const noexcept -> real_t {
    return second_derivative_at_x(x);
  }

  // --------------------------------------------------------------------------
  // Display Curve from U-Space
  // --------------------------------------------------------------------------

  /*
    Evaluate the display curve at shaped velocity u.

    This is the interface used by the constraint solver for inverting
    the display curve to find u_floor.
  */
  auto display_curve_at_u(real_t u, CurveInterpretation interp) const noexcept
      -> real_t {
    const auto x = u * v_to_x_;
    if (interp == CurveInterpretation::kSensitivity) {
      return sensitivity_at_x(x);
    } else {
      return gain_at_x(x);
    }
  }

  /*
    Create a functor for the display curve (for use with
    inverse_via_partition).

    Usage:
      auto curve =
    spline_view.display_curve_functor(CurveInterpretation::kGain);
    auto const y = curve(some_u);  // Evaluate G(u * u_to_x)
  */
  auto display_curve_functor(CurveInterpretation interp) const {
    return [this, interp](real_t u) -> real_t {
      return display_curve_at_u(u, interp);
    };
  }

 private:
  const curves_spline* spline_ = nullptr;
  real_t v_to_x_;
  real_t x_to_v_;

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
    const auto t = std::min(1.0L, fixed_to_real(spline::map_x_to_t(
                                      x_fixed, segment_desc.width_log2)));
    const auto width = static_cast<real_t>(1LL << segment_desc.width_log2);
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
