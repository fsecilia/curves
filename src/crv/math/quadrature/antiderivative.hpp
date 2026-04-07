// SPDX-License-Identifier: MIT

/// \file
/// \brief accumulation function yielding a 1-jet using adaptive quadrature
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <utility>

namespace crv::quadrature {

/// antiderivative, yielding a 1-jet via an accumulation function
///
/// This type is an orchestrator. It encapsulates an integrand, a quadrature rule, and an integral cache to form a
/// continuous accumulation function, F(x).
template <typename real_t, typename integrand_t, typename rule_t, typename cache_t> class antiderivative_t
{
public:
    using jet_t = jet_t<real_t>;

    antiderivative_t() = delete;
    antiderivative_t(integrand_t f, rule_t rule, cache_t cache) noexcept
        : integrand_{std::move(f)}, rule_{std::move(rule)}, cache_{std::move(cache)}
    {}

    /// evaluates accumulation function F(x) and its derivative f(x).
    ///
    /// The primal of the integral is the sum of the nearest cached base integral and a local residual calculated using
    /// the quadrature rule and integrand. The tangent of the integral, by the First Fundamental Theorem of Calculus, is
    /// the original integrand itself, evaluated directly.
    auto operator()(real_t location) const noexcept -> jet_t
    {
        auto const interval = cache_.interval(location);
        auto const residual = rule_(interval.left_bound, location, integrand_);
        auto const integral = interval.base_integral + residual;
        return jet_t{integral, integrand_(location)};
    }

private:
    integrand_t integrand_;
    rule_t      rule_;
    cache_t     cache_;
};

} // namespace crv::quadrature
