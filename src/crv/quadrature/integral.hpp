// SPDX-License-Identifier: MIT

/// \file
/// \brief integral combined from a rule and integrand
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/quadrature/rules.hpp>
#include <utility>

namespace crv::quadrature {

template <typename integral_t, typename scalar_t>
concept is_integral = requires(integral_t const& integral, scalar_t value) {
    typename integral_t::estimate_t;
    { integral.estimate(value, value) } -> std::same_as<typename integral_t::estimate_t>;
    { integral.integrate(value, value) } -> std::same_as<scalar_t>;
};

template <typename t_integrand_t, typename t_rule_t> class integral_t
{
public:
    using integrand_t = t_integrand_t;
    using rule_t = t_rule_t;
    using scalar_t = rule_t::scalar_t;
    using estimate_t = rule_t::estimate_t;

    constexpr integral_t(integrand_t integrand, rule_t rule) noexcept
        : integrand_{std::move(integrand)}, rule_{std::move(rule)}
    {}

    /// integrates over [left, right], returns sum and error
    constexpr auto estimate(scalar_t left, scalar_t right) const noexcept -> estimate_t
    {
        return rule_.estimate(left, right, integrand_);
    }

    /// integrates over [left, right], returns sum
    constexpr auto integrate(scalar_t left, scalar_t right) const noexcept -> scalar_t
    {
        return rule_.integrate(left, right, integrand_);
    }

    /// evaluates integrand at a point
    constexpr auto evaluate_integrand(scalar_t position) const noexcept -> scalar_t { return integrand_(position); }

private:
    integrand_t integrand_;
    rule_t rule_;
};

} // namespace crv::quadrature
