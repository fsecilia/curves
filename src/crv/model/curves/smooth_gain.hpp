// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/model/curve_interpretation.hpp>
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

/// two-zone gain with a log-logistic transition in log gain
///
///     L = log(g_f/g_t)
///     k = 4*elasticity/L
///     z = v/v_50
///     H(v) = z^k/(1 + z^k)
///     g(v) = exp(log(g_t) + L*H(v))
///
/// The curve starts exactly at g_t, reaches the geometric midpoint sqrt(g_t*g_f) at v_50, and approaches g_f
/// asymptotically. `elasticity` is the peak gain elasticity:
///
///     E(v) = v*g'(v)/g(v) = 4*elasticity*H(v)*(1 - H(v))
///     E(v_50) = elasticity
///
/// Valid params require 0 < g_t < g_f, v_50 > 0, elasticity > 0, and k > 1. The final condition keeps the exact
/// origin joined to the low-gain side with zero first derivative.
struct smooth_gain_t
{
    static constexpr auto default_interpretation = curve_interpretation_t::gain;

    template <std::floating_point real_t> struct params_t
    {
        using curve_t = smooth_gain_t;

        real_t g_t;
        real_t g_f;
        real_t v_50;
        real_t elasticity;

        constexpr auto operator==(params_t const&) const noexcept -> bool = default;
    };

    template <std::floating_point t_scalar_t> class evaluator_t
    {
    public:
        using curve_t = smooth_gain_t;
        using scalar_t = t_scalar_t;
        using jet_t = crv::jet_t<scalar_t>;

        explicit evaluator_t(params_t<scalar_t> const& params) noexcept
            : g_t_{params.g_t}, v_50_{params.v_50}, elasticity_{params.elasticity}
        {
            using std::isfinite;
            using std::log;

            assert(isfinite(g_t_) && g_t_ > scalar_t{0} && "smooth_gain_t: g_t must be finite and positive");
            assert(
                isfinite(params.g_f) && params.g_f > g_t_ && "smooth_gain_t: g_f must be finite and greater than g_t");
            assert(isfinite(v_50_) && v_50_ > scalar_t{0} && "smooth_gain_t: v_50 must be finite and positive");
            assert(isfinite(elasticity_) && elasticity_ > scalar_t{0}
                && "smooth_gain_t: elasticity must be finite and positive");

            log_g_t_ = log(g_t_);
            log_gain_delta_ = log(params.g_f) - log_g_t_;
            log_v_50_ = log(v_50_);
            k_ = scalar_t{4} * elasticity_ / log_gain_delta_;

            assert(isfinite(k_) && k_ > scalar_t{1}
                && "smooth_gain_t: elasticity must keep the derived Hill exponent greater than one");
        }

        template <typename value_t> [[nodiscard]] auto operator()(value_t input) const noexcept -> value_t
        {
            using std::exp;

            auto const x = primal(input);
            assert(x >= scalar_t{0} && "smooth_gain_t: input must be nonnegative");

            if (x == scalar_t{0}) return value_t{g_t_};

            auto const transition = transition_value(x);
            auto const gain = exp(log_g_t_ + log_gain_delta_ * transition);

            if constexpr (is_jet<value_t>)
            {
                auto const gain_elasticity = scalar_t{4} * elasticity_ * transition * (scalar_t{1} - transition);
                return {gain, gain * gain_elasticity / x * tangent(input)};
            }
            else return value_t{gain};
        }

        /// finite nonnegative input coordinate
        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {scalar_t{0}, std::numeric_limits<scalar_t>::max()};
        }

        /// no interior critical points
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }

    private:
        /// evaluates the Hill/Naka-Rushton transition without forming z^k
        [[nodiscard]] auto transition_value(scalar_t input) const noexcept -> scalar_t
        {
            using std::exp;
            using std::log;

            auto const logit = k_ * (log(input) - log_v_50_);
            if (logit >= scalar_t{0})
            {
                auto const exp_negative = exp(-logit);
                return scalar_t{1} / (scalar_t{1} + exp_negative);
            }

            auto const exp_positive = exp(logit);
            return exp_positive / (scalar_t{1} + exp_positive);
        }

        scalar_t g_t_;
        scalar_t v_50_;
        scalar_t elasticity_;
        scalar_t log_g_t_{};
        scalar_t log_gain_delta_{};
        scalar_t log_v_50_{};
        scalar_t k_{};
    };

    struct config_t
    {
        using curve_t = smooth_gain_t;

        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> g_t{"g_t", 2.0 / 3.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> g_f{"g_f", 3.0 / 2.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> v_50{"v_50", 5.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> elasticity{
            "elasticity", 1.0};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            self.g_t.reflect(inspector);
            self.g_f.reflect(inspector);
            self.v_50.reflect(inspector);
            self.elasticity.reflect(inspector);
            return std::forward<inspector_t>(inspector);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };
};

/// converts from frontend config to implementation params
///
/// Smooth gain exposes its mathematical parameters directly, so this is a passthrough.
template <std::floating_point real_t>
constexpr auto to_params(smooth_gain_t::config_t const& config) -> smooth_gain_t::params_t<real_t>
{
    return {
        .g_t = static_cast<real_t>(config.g_t.value()),
        .g_f = static_cast<real_t>(config.g_f.value()),
        .v_50 = static_cast<real_t>(config.v_50.value()),
        .elasticity = static_cast<real_t>(config.elasticity.value()),
    };
}

} // namespace crv::model::curves
