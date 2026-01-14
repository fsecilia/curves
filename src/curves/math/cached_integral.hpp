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

//! Composes an integral from an integrand and integrator.
template <typename IntegrandType, typename IntegratorType>
class ComposedIntegral {
 public:
  using Integrand = IntegrandType;
  using Integrator = IntegratorType;
  using Scalar = Integrand::Scalar;

  ComposedIntegral(Integrand integrand, Integrator integrator) noexcept
      : integrand_{std::move(integrand)}, integrator_{std::move(integrator)} {}

  auto integrand() const noexcept -> const Integrand& { return integrand_; }
  auto integrator() const noexcept -> const Integrator& { return integrator_; }

  auto operator()(Scalar right) const noexcept -> Scalar {
    return operator()(Scalar{0}, right);
  }

  auto operator()(Scalar left, Scalar right) const noexcept -> Scalar {
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
                  CompatibleRange<typename Integral::Scalar> auto
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

}  // namespace curves
