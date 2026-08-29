// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/quadrature/antiderivative.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <utility>

namespace crv::shaping::transitions {

namespace construction {
template <std::floating_point scalar_t, typename integrator_t, typename transition_t> class nast_builder_t;
}

namespace detail {

/// non-analytic smooth transition based on the normalized sigmoid form of the non-analytic smooth bump
template <std::floating_point t_scalar_t> struct nast_integrand_t
{
    using scalar_t = t_scalar_t;

    [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return scalar_t{1};

        auto const log_b_over_a = scalar_t{1} / u - scalar_t{1} / (scalar_t{1} - u);
        if (log_b_over_a >= scalar_t{0})
        {
            auto const exp_neg = std::exp(-log_b_over_a);
            return exp_neg / (scalar_t{1} + exp_neg);
        }

        auto const exp_pos = std::exp(log_b_over_a);
        return scalar_t{1} / (scalar_t{1} + exp_pos);
    }
};

template <std::floating_point scalar_t> using nast_rule_t = quadrature::rules::gauss_kronrod_t<scalar_t>;
template <std::floating_point scalar_t>
using nast_integral_t = quadrature::integral_t<nast_integrand_t<scalar_t>, nast_rule_t<scalar_t>>;
template <std::floating_point scalar_t>
using nast_antiderivative_t = quadrature::antiderivative_t<nast_integral_t<scalar_t>>;

} // namespace detail

/// compact C-infinity NAST transition backed by a retained half-domain antiderivative
template <std::floating_point t_scalar_t> class nast_t
{
public:
    using scalar_t = t_scalar_t;
    using jet_t = crv::jet_t<scalar_t>;

    [[nodiscard]] auto operator()(scalar_t u) const noexcept -> scalar_t
    {
        return detail::nast_integrand_t<scalar_t>{}(u);
    }

    [[nodiscard]] auto operator()(jet_t u) const noexcept -> jet_t
    {
        auto const value = primal(u);
        return {operator()(value), derivative(value) * tangent(u)};
    }

    [[nodiscard]] auto derivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0} || u >= scalar_t{1}) return scalar_t{0};

        auto const value = operator()(u);
        if (value == scalar_t{0} || value == scalar_t{1}) return scalar_t{0};

        auto const complement = scalar_t{1} - u;
        auto const logit_slope = scalar_t{1} / (u * u) + scalar_t{1} / (complement * complement);
        return value * (scalar_t{1} - value) * logit_slope;
    }

    [[nodiscard]] auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u <= scalar_t{0}) return scalar_t{0};
        if (u >= scalar_t{1}) return u - support_midpoint;
        if (u <= antiderivative_.domain_end()) return antiderivative_(u);
        return u - support_midpoint + antiderivative_(scalar_t{1} - u);
    }

    [[nodiscard]] auto antiderivative(jet_t u) const noexcept -> jet_t
    {
        auto const value = primal(u);
        return {antiderivative(value), operator()(value) * tangent(u)};
    }

private:
    static constexpr auto support_midpoint = scalar_t{0.5};

    explicit nast_t(detail::nast_antiderivative_t<scalar_t> antiderivative) noexcept
        : antiderivative_{std::move(antiderivative)}
    {
        assert(antiderivative_.domain_end() == support_midpoint && "nast_t: antiderivative must cover half domain");
    }

    template <std::floating_point, typename, typename> friend class construction::nast_builder_t;

    detail::nast_antiderivative_t<scalar_t> antiderivative_;
};

} // namespace crv::shaping::transitions
