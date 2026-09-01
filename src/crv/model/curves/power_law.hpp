// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/domain.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <utility>
#include <vector>

namespace crv::model::curves {

/// power-law curve
///
///     f(x) = (x/p)^g
///     f'(x) = (g/p)(x/p)^(g - 1)
///
/// p is the unit-output speed, f(p) = 1. g is the constant elasticity x*f'(x)/f(x) for x > 0.
///
/// For 0 < g < 1, the value is continuous there but the derivative has a vertical asymptote as x approaches 0. Scalar
/// evaluation is well-defined, but a jet at the origin has a nonfinite tangent for that parameter range.
struct power_law_t
{
    template <std::floating_point real_t> struct params_t
    {
        using curve_t = power_law_t;

        real_t unit_speed;
        real_t power;

        constexpr auto operator==(params_t const&) const noexcept -> bool = default;
    };

    template <std::floating_point t_scalar_t> class evaluator_t
    {
    public:
        using curve_t = power_law_t;
        using scalar_t = t_scalar_t;
        using jet_t = crv::jet_t<scalar_t>;

        explicit evaluator_t(params_t<scalar_t> const& params) noexcept
            : p_{params.unit_speed}, g_{params.power}, log_p_{std::log(params.unit_speed)}
        {
            assert(std::isfinite(p_) && p_ > scalar_t{0} && "power_law_t: p must be finite and positive");
            assert(std::isfinite(g_) && g_ >= scalar_t{0} && "power_law_t: g must be finite and nonnegative");
        }

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t
        {
            assert(input >= scalar_t{0} && "power_law_t: input must be nonnegative");
            if (g_ == scalar_t{0}) return scalar_t{1};
            if (g_ == scalar_t{1}) return input / p_;
            if (input == scalar_t{0}) return scalar_t{0};
            return std::exp(g_ * (std::log(input) - log_p_));
        }

        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t
        {
            auto const x = primal(input);
            assert(x >= scalar_t{0} && "power_law_t: input must be nonnegative");

            if (g_ == scalar_t{0}) return jet_t{scalar_t{1}};
            if (g_ == scalar_t{1}) return input / p_;

            if (x == scalar_t{0})
            {
                if (g_ > scalar_t{1}) return {scalar_t{0}, scalar_t{0}};
                return {scalar_t{0}, tangent(input) * std::numeric_limits<scalar_t>::infinity()};
            }

            auto const y = operator()(x);
            return {y, tangent(input) * g_ * y / x};
        }

        /// finite nonnegative input coordinate
        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {scalar_t{0}, std::numeric_limits<scalar_t>::max()};
        }

        /// no interior critical points
        auto critical_points() const -> std::vector<scalar_t> { return {}; }

    private:
        scalar_t p_;
        scalar_t g_;
        scalar_t log_p_;
    };

    struct config_t
    {
        using curve_t = power_law_t;
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, max<float_t>()>> unit_speed{
            "unit_speed", 1.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 0.0, max<float_t>()>> power{
            "power", 1.0};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            self.unit_speed.reflect(inspector);
            self.power.reflect(inspector);
            return std::forward<inspector_t>(inspector);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };
};

template <std::floating_point real_t>
constexpr auto to_params(power_law_t::config_t const& config) -> power_law_t::params_t<real_t>
{
    return {
        .unit_speed = static_cast<real_t>(config.unit_speed.value()),
        .power = static_cast<real_t>(config.power.value()),
    };
}

} // namespace crv::model::curves
