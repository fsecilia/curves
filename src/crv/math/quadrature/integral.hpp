// SPDX-License-Identifier: MIT

/// \file
/// \brief integral combined from a rule and integrand
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/quadrature/rules.hpp>
#include <concepts>
#include <utility>

namespace crv::quadrature {

template <typename integrand_t, typename rule_t> class integral_t
{
public:
    constexpr integral_t(integrand_t integrand, rule_t rule) noexcept
        : integrand_{std::move(integrand)}, rule_{std::move(rule)}
    {}

    /// integrates over [left, right]
    template <std::floating_point real_t>
    constexpr auto operator()(real_t left, real_t right) const noexcept -> rule_result_t<real_t>
    {
        return rule_(left, right, integrand_);
    }

    /// evaluates integrand at a point
    template <std::floating_point real_t> constexpr auto operator()(real_t position) const noexcept -> real_t
    {
        return integrand_(position);
    }

private:
    integrand_t integrand_;
    rule_t      rule_;
};

} // namespace crv::quadrature
