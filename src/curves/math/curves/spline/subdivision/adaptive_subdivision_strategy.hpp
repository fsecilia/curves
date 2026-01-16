// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Employs adaptive subdivision to subdivide a curve into a spline.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/spline/subdivision/quantization.hpp>
#include <curves/math/curves/spline/subdivision/refinement_queue.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>

namespace curves::spline::segment {

template <typename ErrorEstimator>
class AdaptiveSubdivisionStrategy {
 public:
  using Config = SubdivisionConfig;

  explicit AdaptiveSubdivisionStrategy(ErrorEstimator estimator,
                                       Config config = {})
      : estimator_{std::move(estimator)},
        config_{std::move(config)},
        min_width_{quantize::knot_position(config.segment_width_min)} {}

  template <typename Context, typename Curve, ScalarRange Range>
  auto operator()(Context& context, const Curve& curve,
                  Range&& critical_points) const {
    assert(std::ranges::size(critical_points) >= 2 &&
           "AdaptiveSubdivisionStrategy: Need at least two critical points");

    initialize(context, curve, std::forward<Range>(critical_points));
    subdivide(context, curve);
  }

 private:
  ErrorEstimator estimator_;
  Config config_;
  real_t min_width_;  // quantized

  template <typename Context, typename Curve, typename Range>
  auto initialize(Context& context, const Curve& curve, Range&& points) const
      -> void {
    auto it = std::ranges::begin(points);
    const auto end = std::ranges::end(points);

    // Prepare context and track append location.
    auto tail_segment_index = context.prepare(config_.segments_max);

    // Bootstrap first knot.
    auto prev_knot = make_knot(curve, *it++);

    while (it != end) {
      const auto curr_knot = make_knot(curve, *it++);

      // Skip degenerate segments.
      if (curr_knot.v <= prev_knot.v) continue;

      // Create data.
      const auto segment = make_segment(curve, prev_knot, curr_knot);
      context.segments.push_back(segment);

      // Update topology.
      tail_segment_index =
          context.successor_map.insert_after(tail_segment_index);

      // Add to work queue.
      if (should_split(segment)) {
        context.queue.push(SegmentError{.error = segment.max_error,
                                        .index = tail_segment_index});
      }

      prev_knot = curr_knot;
    }
  }

  template <typename Context, typename Curve>
  auto subdivide(Context& context, const Curve& curve) const noexcept -> void {
    // Iterate segments with errors above tolerance until perfect or full.
    while (!context.refinement_queue.empty()) {
      // Check capacity. We need room for 1 extra segment to hold a split.
      if (context.segments.size() >=
          numeric_cast<std::size_t>(config_.segments_max)) {
        break;
      }

      // Pop worst segment.
      const auto parent_id = context.refinement_queue.pop();
      auto& parent_seg = context.segments[parent_id];
      if (!should_split(parent_seg)) continue;

      // Find where to split.
      const auto v_split =
          clamp_split(parent_seg.start.v, parent_seg.end.v, parent_seg.v_split);
      const auto split_knot = make_knot(curve, v_split);

      // Split.
      const auto left_seg = make_segment(curve, parent_seg.start, split_knot);
      const auto right_seg = make_segment(curve, split_knot, parent_seg.end);

      // Add segments to pool.
      context.segments[parent_id] = left_seg;  // Reuse parent slot.
      context.segments.push_back(right_seg);   // Add right to new slot.

      // Update topology.
      const auto right_id = context.successor_map.insert_after(parent_id);

      // Requeue if necessary.
      if (should_split(left_seg)) {
        context.refinement_queue.push(
            SegmentError{.error = left_seg.max_error, .index = parent_id});
      }
      if (should_split(right_seg)) {
        context.refinement_queue.push(
            SegmentError{.error = right_seg.max_error, .index = right_id});
      }
    }
  }

  template <typename Curve>
  auto make_knot(const Curve& curve, real_t v) const {
    auto v_q = quantize::knot_position(v);
    return Knot{v_q, curve(math::Jet<real_t>{v_q, 1})};
  }

  template <typename Curve, typename Knot>
  auto make_segment(const Curve& curve, const Knot& start,
                    const Knot& end) const noexcept {
    auto width = end.v - start.v;
    auto poly_raw = cubic::hermite_to_monomial(start.y, end.y, width);
    auto poly = quantize::polynomial(poly_raw);

    auto [v_split, max_error] = estimator_(curve, poly, start.v, width);

    // Construct segment.
    return Segment{start, end, poly, max_error, v_split};
  }

  auto should_split(const Segment& seg) const noexcept -> bool {
    return seg.max_error > config_.error_tolerance &&
           seg.width() >= 2 * min_width_;
  }

  auto clamp_split(real_t start, real_t end, real_t split_hint) const
      -> real_t {
    auto v = quantize::knot_position(split_hint);
    return std::clamp(v, start + min_width_, end - min_width_);
  }
};

}  // namespace curves::spline::segment
