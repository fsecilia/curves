// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Builds array of spline knots from a curve using adaptive subdivision.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/spline/subdivision/knot.hpp>
#include <curves/math/curves/spline/subdivision/sampled_error_estimator.hpp>
#include <curves/ranges.hpp>
#include <queue>
#include <utility>

namespace curves {

/*
  These constants should come from a c acl, but since we're doing construction
  first, we define them here for now.
*/
constexpr auto segments_max = 256;
constexpr auto segment_width_min = 1e-5;  // knots are Q8.24
constexpr auto segment_tolerance = 1e-6;  // coeffs are Q0.45 and Q0.46

template <typename ErrorEstimator>
class AdaptiveSubdivider {
 public:
  explicit AdaptiveSubdivider(ErrorEstimator estimate_error) noexcept
      : estimate_error_{std::move(estimate_error)} {}

  template <typename Curve>
  auto operator()(const Curve& curve,
                  CompatibleRange<typename Curve::Scalar> auto critical_points)
      const -> Knots<typename Curve::Scalar> {
    using Scalar = Curve::Scalar;
    using Knot = Knot<Scalar>;

    struct Segment {
      // knots positions in Q8.24
      std::int64_t k0_q8_24;
      std::int64_t k1_q8_24;

      cubic::Monomial<Scalar> poly;
      Scalar max_error;
      Scalar v_split;

      auto operator<(const Segment& other) const noexcept -> bool {
        return max_error < other.max_error;
      }
    };

    // Holds segments that are good enough or can't be split further.
    std::vector<Segment> finalized;
    finalized.reserve(segments_max);

    // Backs the queue so we can reserve upfront.
    std::vector<Segment> pq_container;
    pq_container.reserve(segments_max);

    auto pq = std::priority_queue<Segment, std::vector<Segment>>{
        std::less<>{}, std::move(pq_container)};

    const auto to_q8_24 = [](Scalar v) {
      return static_cast<int64_t>(std::round(v * (1 << 24)));
    };
    const auto q8_24_to_real = [](int64_t v) {
      return static_cast<long double>(v) / (1 << 24);
    };
    const auto segment_min_width_q8_24 = to_q8_24(segment_width_min);

    // Measures and pushes.
    const auto push_job = [&](const Knot& start, const Knot& end) {
      const auto segment_width = end.v - start.v;
      const auto poly =
          cubic::hermite_to_monomial(start.y, end.y, segment_width);
      const auto [max_error, v_split] =
          estimate_error_(curve, poly, start.v, segment_width);

      pq.push({to_q8_24(start.v), to_q8_24(end.v), poly, max_error, v_split});
    };

    // Initialize with required intervals
    auto j0 = curve(math::Jet<Scalar>{critical_points[0], 1.0});
    for (size_t i = 1; i < critical_points.size() - 1; ++i) {
      const auto j1 = curve(math::Jet<Scalar>{critical_points[i + 1], 1.0});
      push_job({critical_points[i], j0}, {critical_points[i + 1], j1});
      j0 = j1;
    }

    // Keep splitting until we max out segments, or run out.
    while (!pq.empty() && (pq.size() + finalized.size() < segments_max)) {
      // Pop segment with worst error.
      auto top = pq.top();
      pq.pop();

      // Is it already within tolerance?
      if (top.error <= segment_tolerance) {
        // Push and go.
        finalized.push_back(top);
        continue;
      }

      // Is it too small to split?
      const auto segment_width_q8_24 = top.k1_q8_24 - top.k0_q8_24;
      if (segment_width_q8_24 < 2 * segment_min_width_q8_24) {
        // Accept it as "best effort".
        finalized.push_back(top);
        continue;
      }

      // Quantize and clamp split location.
      const auto split_q8_24 = to_q8_24(top.v_split);
      const auto clamped_split_q8_24 =
          std::clamp(split_q8_24, segment_min_width_q8_24,
                     segment_width_q8_24 - segment_min_width_q8_24);

      // Split.
      const auto midpoint_v = q8_24_to_real(clamped_split_q8_24);
      const auto midpoint_jet = curve(midpoint_v);
      const auto midpoint_knot = Knot{midpoint_v, midpoint_jet};

      push_job(top.k0, midpoint_knot);
      push_job(midpoint_knot, top.k1);
    }

    // Drain remaining jobs to finalized.
    while (!pq.empty()) {
      finalized.push_back(pq.top());
      pq.pop();
    }

    // Sort and Flatten
    std::sort(finalized.begin(), finalized.end(),
              [](const Segment& a, const Segment& b) {
                return a.k0_q8_24 < b.k0_q8_24;
              });

    std::vector<Knot> result;
    result.reserve(finalized.size() + 1);

    if (!finalized.empty()) {
      result.push_back(q8_24_to_real(finalized[0].k0));
      for (const auto& segment : finalized) {
        result.push_back(q8_24_to_real(segment.k1));
      }
    }

    return result;
  }

 private:
  ErrorEstimator estimate_error_;
};

}  // namespace curves
