// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Applies adaptive subdivison to generate cubic hermite splines.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/math/curves/spline/subdivision/adaptive_subdivision_strategy.hpp>
#include <curves/math/curves/spline/subdivision/quantization.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <curves/math/curves/spline/subdivision/subdivision_context.hpp>
#include <curves/math/jet.hpp>
#include <curves/numeric_cast.hpp>
#include <curves/ranges.hpp>
#include <algorithm>
#include <cassert>
#include <utility>

namespace curves {

// ============================================================================
// Subdivider
// ============================================================================

template <typename Context, typename Strategy>
class Subdivider {
 public:
  explicit Subdivider(Context context, Strategy strategy) noexcept
      : context_{std::move(context)}, strategy_{std::move(strategy)} {}

  template <typename Curve>
  auto operator()(const Curve& curve, ScalarRange auto&& critical_points) const
      -> QuantizedSpline {
    assert(std::ranges::size(critical_points) >= 2 &&
           "Need at least two critical points");

    strategy_(context_, curve,
              std::forward<decltype(critical_points)>(critical_points));

    return extract_result();
  }

 private:
  mutable Context context_;
  Strategy strategy_;

  auto extract_result() const -> QuantizedSpline {
    QuantizedSpline out;
    out.knots.reserve(context_.segments.size() + 1);
    out.polys.reserve(context_.segments.size());

    auto id = context_.successor_map.head();
    if (id != SegmentIndex::Null) {
      // Push first knot.
      auto index = std::to_underlying(id);
      out.knots.push_back(context_.segments[index].start.v);

      while (id != SegmentIndex::Null) {
        const auto& seg = context_.segments[index];
        out.polys.push_back(seg.poly);
        out.knots.push_back(seg.end.v);
        id = context_.successor_map.successor(id);
        index = std::to_underlying(id);
      }
    }
    return out;
  }
};

// ============================================================================
// Factory
// ============================================================================

template <typename ErrorEstimator>
auto make_adaptive_subdivider(ErrorEstimator estimator,
                              SubdivisionConfig config = {}) -> auto {
  using Queue = spline::segment::RefinementQueue<spline::segment::SegmentError>;
  using Map = spline::segment::SuccessorMap;
  using Context = spline::segment::SubdivisionContext<Queue, Map>;
  using Strat = spline::segment::AdaptiveSubdivisionStrategy<ErrorEstimator>;

  return Subdivider<Context, Strat>{
      Context{
          .segments = {}, .refinement_queue = Queue{}, .successor_map = Map{}},
      Strat{std::move(estimator), std::move(config)}};
}

}  // namespace curves
