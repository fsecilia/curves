// SPDX-License-Identifier: MIT

/// \file
/// \brief cached antiderivative and conditioned integrand-prefix evaluation using adaptive quadrature
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/quadrature/antiderivative_cache.hpp>
#include <cassert>
#include <utility>

namespace crv::quadrature {

/// antiderivative, yielding a 1-jet via an accumulation function
template <typename t_integral_t> class antiderivative_t
{
public:
    using integral_t = t_integral_t;
    using scalar_t = integral_t::scalar_t;
    using jet_t = jet_t<scalar_t>;
    using cache_t = antiderivative_cache_t<scalar_t>;

    constexpr antiderivative_t(integral_t integral, cache_t cache) noexcept
        : integral_{std::move(integral)}, cache_{std::move(cache)}
    {}

    /// evaluates accumulation function with a scalar, returning F(x)
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return integrate(x); }

    /// evaluates F(x) and derivative f(x) with a jet
    ///
    /// F(x) is the nearest cached prefix plus a local residual integral. Its derivative is the original integrand
    /// evaluated directly.
    constexpr auto operator()(jet_t x) const noexcept -> jet_t
    {
        auto const primal_x = primal(x);
        return jet_t{integrate(primal_x), integral_.evaluate_integrand(primal_x) * tangent(x)};
    }

    /// evaluates the derivative of the antiderivative, which is the retained integrand
    constexpr auto derivative(scalar_t x) const noexcept -> scalar_t
    {
        assert_domain(x);
        return integral_.evaluate_integrand(x);
    }

    /// evaluates mean integrand over [0, x]
    ///
    /// Directly computing F(x) / x would magnify fixed absolute integration error near zero. Let a be the previous
    /// cached boundary, g_a = F(a) / a, and m(a,x) the mean over the uncached remainder. Then
    ///
    ///     G(x) = m(a,x) + (a/x) * (g_a - m(a,x)).
    ///
    /// Since a/x is in [0,1], this stays well conditioned. In the first interval a == 0, so only the residual mean is
    /// needed. At x == 0, the continuous mean is f(0).
    constexpr auto mean_integrand(scalar_t x) const noexcept -> scalar_t
    {
        assert_domain(x);

        if (x == scalar_t{0}) return derivative(x);

        auto const cached = cache_.lookup(x);

        // exact cache boundary needs no residual evaluation
        if (x == cached.boundary) return cached.cumulative_sum / cached.boundary;

        auto const residual_mean = integral_.average(cached.boundary, x);
        if (cached.boundary == scalar_t{0}) return residual_mean;

        auto const cached_prefix_mean = cached.cumulative_sum / cached.boundary;
        auto const prefix_fraction = cached.boundary / x;
        return residual_mean + prefix_fraction * (cached_prefix_mean - residual_mean);
    }

    /// right edge of the cached domain
    constexpr auto domain_end() const noexcept -> scalar_t { return cache_.domain_end(); }

    /// number of accepted quadrature segments
    constexpr auto segment_count() const noexcept -> int_t { return cache_.segment_count(); }

private:
    constexpr auto integrate(scalar_t x) const noexcept -> scalar_t
    {
        auto const cached = cache_.lookup(x);
        auto const residual = integral_.integrate(cached.boundary, x);
        return cached.cumulative_sum + residual;
    }

    constexpr auto assert_domain([[maybe_unused]] scalar_t x) const noexcept -> void
    {
        assert(cache_.contains(x) && "antiderivative_t: domain error");
    }

    integral_t integral_;
    cache_t cache_;
};

} // namespace crv::quadrature
