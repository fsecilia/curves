// SPDX-License-Identifier: MIT

/// \file
/// \brief integral combined from a rule and integrand
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/quadrature/rules.hpp>
#include <utility>

namespace crv::quadrature {

template <typename t_integrand_t, typename t_rule_t> class integral_t
{
public:
    using integrand_t = t_integrand_t;
    using rule_t = t_rule_t;
    using real_t = rule_t::real_t;
    using estimate_t = rule_t::estimate_t;

    constexpr integral_t(integrand_t integrand, rule_t rule) noexcept
        : integrand_{std::move(integrand)}, rule_{std::move(rule)}
    {}

    /// integrates over [left, right], returns sum and error
    constexpr auto estimate(real_t left, real_t right) const noexcept -> estimate_t
    {
        return rule_.estimate(left, right, integrand_);
    }

    /// integrates over [left, right], returns sum
    constexpr auto integrate(real_t left, real_t right) const noexcept -> real_t
    {
        return rule_.integrate(left, right, integrand_);
    }

    /// evaluates integrand at a point
    constexpr auto evaluate_integrand(real_t position) const noexcept -> real_t { return integrand_(position); }

private:
    integrand_t integrand_;
    rule_t rule_;
};

} // namespace crv::quadrature
