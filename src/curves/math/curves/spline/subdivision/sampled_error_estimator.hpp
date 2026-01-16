// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Estimates segment error by sampling a set of candidate locations.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cmath>
#include <ostream>

namespace curves {

//! Maximum estimated error and its curve location.
struct SegmentErrorEstimate {
  real_t v;
  real_t error;

  friend auto operator<<(std::ostream& out, const SegmentErrorEstimate& src)
      -> std::ostream& {
    return out << "SegmentErrorEstimate{.v = " << src.v
               << ", .error = " << src.error << "}";
  }
};

/*!
  Estimates a segment's maximum error and its location by sampling a set of
  candidate locations.

  This type takes a set of candidate locations and compares the value of the
  generating curve there against the value of the spline segment approximating
  it. It returns the argmax of curve location and error.
*/
template <typename ErrorCandidateLocator>
struct SampledErrorEstimator {
  ErrorCandidateLocator locate_error_candidates;

  template <typename Curve, typename Segment>
  auto operator()(const Curve& curve, const Segment& segment, real_t v0,
                  real_t segment_width) const noexcept -> SegmentErrorEstimate {
    const auto candidates = locate_error_candidates(segment);

    // Argmax candidates to find max err and the v that produces it.
    auto max_err = 0.0;
    auto v_max_err = v0 + 0.5 * segment_width;  // Default to midpoint.
    for (const auto t_candidate : candidates) {
      const auto y_approximation = segment(t_candidate);
      const auto v_t = v0 + t_candidate * segment_width;
      const auto y_true = curve(v_t);

      const auto err = std::abs(y_approximation - y_true);
      if (err > max_err) {
        max_err = err;
        v_max_err = v_t;
      }
    }

    return {v_max_err, max_err};
  }
};

}  // namespace curves
