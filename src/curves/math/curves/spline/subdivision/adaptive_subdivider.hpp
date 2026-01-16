// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Applies adaptive subdivison to generate cubic hermite splines.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/math/curves/spline/subdivision/quantization.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <curves/math/jet.hpp>
#include <curves/numeric_cast.hpp>
#include <curves/ranges.hpp>
#include <algorithm>
#include <cassert>
#include <queue>
#include <vector>

namespace curves {

// ============================================================================
// Configuration
// ============================================================================

struct SubdivisionConfig {
  int_t segments_max = 256;
  real_t segment_width_min = 1.0 / (1 << 16);  // 2^-16
  real_t error_tolerance = 1e-6;
};

// ============================================================================
// Subdivision Types
// ============================================================================

struct QuantizedKnot {
  real_t v;
  math::Jet<real_t> y;
};

struct Segment {
  QuantizedKnot start;
  QuantizedKnot end;
  cubic::Monomial poly;
  real_t max_error;
  real_t v_split;

  auto width() const noexcept -> real_t { return end.v - start.v; }

  auto operator<(const Segment& other) const noexcept -> bool {
    return max_error < other.max_error;
  }
};

// ============================================================================
// Subdivision Result
// ============================================================================

/*!
  Output of adaptive subdivision, ready for spline construction.

  Contains parallel arrays of knot positions and segment polynomials.
  knots.size() == segments.size() + 1.
*/
struct SubdivisionResult {
  std::vector<real_t> knots;           // Quantized positions, Q8.24
  std::vector<cubic::Monomial> polys;  // Quantized coefficients

  auto segment_count() const noexcept -> int_t {
    return numeric_cast<int_t>(polys.size());
  }
};

// ============================================================================
// Adaptive Subdivider
// ============================================================================

template <typename ErrorEstimator>
class AdaptiveSubdivider {
 public:
  explicit AdaptiveSubdivider(ErrorEstimator estimate_error,
                              SubdivisionConfig config = {}) noexcept
      : estimate_error_{std::move(estimate_error)}, config_{config} {}

  template <typename Curve>
  auto operator()(const Curve& curve, ScalarRange auto&& critical_points) const
      -> SubdivisionResult {
    assert(std::ranges::size(critical_points) >= 2 &&
           "Need at least two critical points");

    auto subdivider = Subdivider<Curve>{curve, estimate_error_, config_};
    return subdivider(std::forward<decltype(critical_points)>(critical_points));
  }

 private:
  ErrorEstimator estimate_error_;
  SubdivisionConfig config_;

  // -------------------------------------------------------------------------
  // Stateful Implementation
  // -------------------------------------------------------------------------

  template <typename Curve>
  class Subdivider {
   public:
    using Knot = QuantizedKnot;
    using Segment = Segment;
    using Queue = std::priority_queue<Segment>;

    Subdivider(const Curve& curve, const ErrorEstimator& estimator,
               const SubdivisionConfig& config) noexcept
        : curve_{curve},
          estimator_{estimator},
          config_{config},
          min_width_{quantize::knot_position(config.segment_width_min)} {
      finalized_.reserve(config.segments_max);
    }

    template <typename Range>
    auto operator()(Range&& critical_points) -> SubdivisionResult {
      initialize(std::forward<Range>(critical_points));
      subdivide();
      return result();
    }

   private:
    const Curve& curve_;
    const ErrorEstimator& estimator_;
    const SubdivisionConfig& config_;
    const real_t min_width_;

    Queue queue_;
    std::vector<Segment> finalized_;

    //! Seeds the queue from critical points.
    template <typename Range>
    void initialize(Range&& critical_points) {
      auto it = std::ranges::begin(critical_points);
      const auto end = std::ranges::end(critical_points);

      auto prev = make_knot(*it);
      ++it;

      while (it != end) {
        auto curr = make_knot(*it++);

        // Skip degenerate segments where critical points quantized together.
        if (curr.v <= prev.v) {
          continue;
        }

        queue_.push(make_segment(prev, curr));
        prev = curr;
      }
    }

    //! Runs the subdivision loop until done or at capacity.
    void subdivide() {
      while (!queue_.empty() && has_capacity()) {
        auto seg = queue_.top();
        queue_.pop();

        if (should_accept(seg)) {
          finalized_.push_back(std::move(seg));
        } else {
          split(std::move(seg));
        }
      }

      drain_remaining();
    }

    //! Extracts the final result.
    auto result() -> SubdivisionResult {
      sort_by_position();
      return extract_result();
    }

    // -----------------------------------------------------------------------
    // Knot and Segment Creation
    // -----------------------------------------------------------------------

    auto make_knot(real_t v) const -> Knot {
      const auto v_q = quantize::knot_position(v);
      const auto jet = curve_(math::Jet<real_t>{v_q, 1});
      return Knot{v_q, jet};
    }

    auto make_segment(const Knot& start, const Knot& end) const -> Segment {
      const auto width = end.v - start.v;

      // Fit Hermite polynomial and quantize coefficients.
      const auto poly_raw = cubic::hermite_to_monomial(start.y, end.y, width);
      const auto poly = quantize::polynomial(poly_raw);

      // Estimate error of the quantized polynomial against the curve.
      const auto [v_split, max_error] =
          estimator_(curve_, poly, start.v, width);

      return Segment{start, end, poly, max_error, v_split};
    }

    // -----------------------------------------------------------------------
    // Decision Logic
    // -----------------------------------------------------------------------

    auto has_capacity() const noexcept -> bool {
      // Need room for two children if we split.
      return queue_.size() + finalized_.size() + 1 <
             numeric_cast<size_t>(config_.segments_max);
    }

    auto should_accept(const Segment& seg) const noexcept -> bool {
      // Accept if error is tolerable.
      if (seg.max_error <= config_.error_tolerance) {
        return true;
      }
      // Accept if too narrow to split.
      if (seg.width() < 2 * min_width_) {
        return true;
      }
      return false;
    }

    // -----------------------------------------------------------------------
    // Splitting
    // -----------------------------------------------------------------------

    void split(Segment seg) {
      // Quantize split point and clamp to ensure valid children.
      auto v_mid = quantize::knot_position(seg.v_split);
      v_mid =
          std::clamp(v_mid, seg.start.v + min_width_, seg.end.v - min_width_);

      const auto mid = make_knot(v_mid);

      queue_.push(make_segment(seg.start, mid));
      queue_.push(make_segment(mid, seg.end));
    }

    void drain_remaining() {
      while (!queue_.empty()) {
        finalized_.push_back(queue_.top());
        queue_.pop();
      }
    }

    // -----------------------------------------------------------------------
    // Result Extraction
    // -----------------------------------------------------------------------

    void sort_by_position() {
      std::ranges::sort(finalized_, [](const Segment& a, const Segment& b) {
        return a.start.v < b.start.v;
      });
    }

    auto extract_result() const -> SubdivisionResult {
      SubdivisionResult out;
      out.knots.reserve(finalized_.size() + 1);
      out.polys.reserve(finalized_.size());

      for (const auto& seg : finalized_) {
        out.knots.push_back(seg.start.v);
        out.polys.push_back(seg.poly);
      }

      if (!finalized_.empty()) {
        out.knots.push_back(finalized_.back().end.v);
      }

      return out;
    }
  };
};

// ============================================================================
// Factory
// ============================================================================

template <typename ErrorEstimator>
auto make_adaptive_subdivider(ErrorEstimator estimator,
                              SubdivisionConfig config = {})
    -> AdaptiveSubdivider<ErrorEstimator> {
  return AdaptiveSubdivider<ErrorEstimator>{std::move(estimator), config};
}

}  // namespace curves
