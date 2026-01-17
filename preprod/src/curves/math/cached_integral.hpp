// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Adaptive quadrature integral cache.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/jet.hpp>
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
// Integrals
// ----------------------------------------------------------------------------

/*!
  Composes an integral from an integrand and integrator.

  This type is the live integral. It uses an integrator like Gauss5 to
  calculate a single interval by evaluating the integrand multiple times. It
  becomes less accurate with wider intervals.
*/
template <typename IntegrandType, typename IntegratorType>
class ComposedIntegral {
 public:
  using Integrand = IntegrandType;
  using Integrator = IntegratorType;

  ComposedIntegral(Integrand integrand, Integrator integrator) noexcept
      : integrand_{std::move(integrand)}, integrator_{std::move(integrator)} {}

  auto integrand() const noexcept -> const Integrand& { return integrand_; }
  auto integrator() const noexcept -> const Integrator& { return integrator_; }

  auto operator()(real_t right) const noexcept -> real_t {
    return operator()(0.0, right);
  }

  auto operator()(real_t left, real_t right) const noexcept -> real_t {
    return integrator_(integrand_, left, right);
  }

 private:
  Integrand integrand_;
  Integrator integrator_;
};

//! Creates ComposedIntegrals from an integrand and integrator.
template <typename Integrator>
struct ComposedIntegralFactory {
  Integrator integrator;

  template <typename Integrand>
  auto operator()(Integrand integrand) const {
    return ComposedIntegral<Integrand, Integrator>{std::move(integrand),
                                                   integrator};
  }
};

// ----------------------------------------------------------------------------
// Cached Integral
// ----------------------------------------------------------------------------

/*!
  Calculates integrals using cached samples + residuals.

  This type uses a cache to look up a nearby result, then calls out to the
  integral to calculate the rest of the interval, and returns the sum. It
  still costs an integration per bound, but it is guaranteed to be within
  whatever accuracy was specified with building the cache, since it only runs
  over the residual interval.
*/
template <typename Integral>
class CachedIntegral {
 public:
  //! Maps sample locations to prefix sums at those locations.
  using Cache = std::flat_map<real_t, real_t>;

  CachedIntegral(Integral integral, Cache cache) noexcept
      : integral_{std::move(integral)}, cache_{std::move(cache)} {
    assert(!cache_.empty() && "CachedIntegral: empty boundaries");
    assert(cache_.keys().front() == 0.0 &&
           "CachedIntegral: sample locations must start at 0");
    assert(cache_.values().front() == 0.0 &&
           "CachedIntegral: prefix sums must start at 0");
  }

  //! \returns integral from 0 to location; integrates once
  template <typename Value>
  auto operator()(Value location) const noexcept -> Value {
    assert(cache_.keys().front() <= location &&
           location <= cache_.keys().back() && "CachedIntegral: domain error");

    const auto right_boundary = cache_.upper_bound(location);
    const auto left_boundary = std::ranges::prev(right_boundary);

    const auto cached_sample = left_boundary->second;
    const auto residual = integral_(left_boundary->first, location);
    return cached_sample + residual;
  }

  //! \returns integral from left to right; integrates twice
  auto operator()(real_t left, real_t right) const noexcept -> real_t {
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

/*!
  Constructs integral cache using adaptive quadrature.

  This implementation takes a set of critical points, then splits the intervals
  between them until the integrals across them are below a certain tolerance.

  It is also has a simple constraint that it won't split an interval more than
  64 times. A region that is that difficult to integrate will be less accurate,
  but it won't consume the entire heap. It's just a contingency and shouldn't
  happen in normal usage.
*/
class CachedIntegralBuilder {
 public:
  /*!
    Caches integral in the domain [0, max] to within tolerance.

    \pre critical_points in [0, max]
    \pre critical_points sorted
  */
  template <typename Integral>
  auto operator()(Integral integral, real_t max, real_t tolerance,
                  ScalarRange auto critical_points) const noexcept
      -> CachedIntegral<Integral> {
    auto boundaries = std::vector<real_t>{0};
    auto cumulative = std::vector<real_t>{0};

    struct Interval {
      real_t left;
      real_t right;
      real_t integral_sum;
      int_t depth;
    };
    auto pending_intervals = std::vector<Interval>{};

    // Seed pending intervals from critical points.
    auto seed_interval_max = max;
    for (const auto critical_point : std::views::reverse(critical_points)) {
      assert(critical_point <= seed_interval_max &&
             "CachedIntegralBuilder: Critical points must be sorted");
      pending_intervals.emplace_back(
          critical_point, seed_interval_max,
          integral(critical_point, seed_interval_max), 0);
      seed_interval_max = critical_point;
    }
    pending_intervals.emplace_back(0, seed_interval_max,
                                   integral(0.0, seed_interval_max), 0);

    /*
      The contingency should be more flexible in the future, but since it also
      should never fire, configuring it locally is arguably not terrible.
    */
    static constexpr auto kMaxDepth = 64;

    // Run adaptive quadrature.
    auto total_area = KahanAccumulator<real_t>{};
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
        /*
          The contingency is only there because we can't predict all curve
          parameterizations. It shouldn't happen, but if it does, make sure
          someone knows.
        */
        assert(depth < kMaxDepth &&
               "CachedIntegralBuilder: max depth exceeded");

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

}  // namespace curves
