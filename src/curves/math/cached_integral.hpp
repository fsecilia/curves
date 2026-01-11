// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Adaptive quadrature integral cache.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/kahan_accumulator.hpp>
#include <curves/ranges.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <flat_map>
#include <numeric>
#include <utility>
#include <vector>

namespace curves {

// ----------------------------------------------------------------------------
// Cached Integral
// ----------------------------------------------------------------------------

//! Calculates integrals using cached samples + residuals.
template <typename Integral>
class CachedIntegral {
 public:
  using Scalar = Integral::Scalar;

  //! Maps sample locations to prefix sums at those locations.
  using Cache = std::flat_map<Scalar, Scalar>;

  CachedIntegral(Integral integral, Cache cache) noexcept
      : integral_{std::move(integral)}, cache_{std::move(cache)} {
    assert(!cache_.empty() && "CachedIntegral: empty boundaries");
    assert(cache_.keys().front() == Scalar{0} &&
           "CachedIntegral: sample locations must start at 0");
    assert(cache_.values().front() == Scalar{0} &&
           "CachedIntegral: prefix sums must start at 0");
  }

  //! \returns integral from 0 to location
  auto operator()(Scalar location) const noexcept -> Scalar {
    assert(cache_.keys().front() <= location &&
           location <= cache_.keys().back() && "CachedIntegral: domain error");

    const auto right_boundary = cache_.upper_bound(location);
    const auto left_boundary = std::ranges::prev(right_boundary);
    return left_boundary->second + integral_(left_boundary->first, location);
  }

  //! \returns integral from left to right
  auto operator()(Scalar left, Scalar right) const noexcept -> Scalar {
    return operator()(right) - operator()(left);
  }

  auto integral() const noexcept -> const Integral& { return integral_; }
  auto cache() const noexcept -> const Cache& { return cache_; }

 private:
  Integral integral_;
  Cache cache_;
};

// ----------------------------------------------------------------------------
// Cached Integral Builder
// ----------------------------------------------------------------------------

//! Constructs integral cache using adaptive quadrature.
class CachedIntegralBuilder {
 public:
  /*!
    Caches integral in the domain [0, max] to within tolerance.

    \pre critical_points in [0, max]
    \pre critical_points sorted
  */
  template <typename Integral>
  auto operator()(Integral integral, Integral::Scalar max,
                  Integral::Scalar tolerance,
                  const CompatibleRange<typename Integral::Scalar> auto&
                      critical_points) const noexcept
      -> CachedIntegral<Integral> {
    using Scalar = Integral::Scalar;

    auto boundaries = std::vector<Scalar>{0};
    auto cumulative = std::vector<Scalar>{0};

    struct Interval {
      Scalar left;
      Scalar right;
      Scalar integral_sum;
      int_t depth;
    };
    auto pending_intervals = std::vector<Interval>{};

    // Seed pending intervals from critical points.
    auto seed_interval_max = max;
    for (const auto critical_point : std::views::reverse(critical_points)) {
      assert(critical_point < seed_interval_max &&
             "CachedIntegralBuilder: Critical points must be sorted");
      pending_intervals.emplace_back(
          critical_point, seed_interval_max,
          integral(critical_point, seed_interval_max), 0);
      seed_interval_max = critical_point;
    }
    pending_intervals.emplace_back(0, seed_interval_max,
                                   integral(Scalar{0}, seed_interval_max), 0);

    static constexpr auto kMaxDepth = 64;

    // Run adaptive quadrature.
    auto total_area = KahanAccumulator<Scalar>{};
    while (!pending_intervals.empty()) {
      // Get leftmost pending interval.
      const auto [left, right, coarse, depth] =
          std::move(pending_intervals.back());
      pending_intervals.pop_back();

      // Evaluate integrals for both halves.
      const auto midpoint = std::midpoint(left, right);
      const auto left_integral = integral(left, midpoint);
      const auto right_integral = integral(midpoint, right);
      const auto refined = left_integral + right_integral;

      // Accumulate or subdivide.
      const auto converged = std::abs(refined - coarse) < tolerance;
      if (converged || depth >= kMaxDepth) {
        // Value is within tolerance. Accumulate it.
        boundaries.push_back(right);
        total_area += refined;
        cumulative.push_back(total_area);
      } else {
        // Subdivide. Push right first to maintain left-to-right order.
        pending_intervals.push_back({midpoint, right, right_integral});
        pending_intervals.push_back({left, midpoint, left_integral});
      }
    }

    using Result = CachedIntegral<Integral>;
    using Cache = typename Result::Cache;
    return Result{std::move(integral),
                  Cache{std::sorted_unique, std::move(boundaries),
                        std::move(cumulative)}};
  }
};

// ----------------------------------------------------------------------------
// Integrals
// ----------------------------------------------------------------------------

//! Adapts a numerical integrator around an integrand to present an integral.
template <typename Integrand, typename Integrator>
struct ComposedIntegral {
  using Scalar = Integrand::Scalar;

  Integrand integrand;
  Integrator integrator;

  template <typename Value>
  auto operator()(Value left, Value right) const noexcept -> Value {
    return integrator(integrand, left, right);
  }
};

}  // namespace curves
