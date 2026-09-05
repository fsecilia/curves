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

/// transitions positive gain in log space with a Naka-Rushton profile
///
/// The curve is exactly g_t at and below v_0. Above v_0:
///
///     z = (v - v_0)/(v_50 - v_0)
///     H(z) = z^k/(1 + z^k)
///     g(v) = g_t*(g_f/g_t)^H(z) = exp(log(g_t) + (log(g_f) - log(g_t))*H(z))
///     E_g = k*ln(g_f/g_t)H(v)(1 - H(v))
///
/// v_50 is the point where half of the log-gain change is complete, so g(v_50) = sqrt(g_t*g_f). The final gain g_f
/// is approached asymptotically. k controls transition concentration; k >= 2 keeps the exact lower join at least C1.
struct smooth_gain_t
{
    static constexpr auto default_interpretation = curve_interpretation_t::gain;

    template <std::floating_point real_t> struct params_t
    {
        using curve_t = smooth_gain_t;

        real_t v_0;
        real_t v_50;
        real_t k;
        real_t g_t;
        real_t g_f;

        constexpr auto operator==(params_t const&) const noexcept -> bool = default;
    };

    template <std::floating_point t_scalar_t> class evaluator_t
    {
    public:
        using curve_t = smooth_gain_t;
        using scalar_t = t_scalar_t;
        using jet_t = crv::jet_t<scalar_t>;

        explicit evaluator_t(params_t<scalar_t> const& params) noexcept
            : v_0_{params.v_0}, v_50_{params.v_50}, k_{params.k}, g_t_{params.g_t}, g_f_{params.g_f}
        {
            using std::isfinite;
            using std::log;

            assert(isfinite(v_0_) && "smooth_gain_t: v_0 must be finite");
            assert(isfinite(v_50_) && v_50_ > v_0_ && "smooth_gain_t: v_50 must be finite and greater than v_0");
            assert(isfinite(k_) && k_ >= scalar_t{2} && "smooth_gain_t: k must be finite and at least 2");
            assert(isfinite(g_t_) && g_t_ > scalar_t{0} && "smooth_gain_t: g_t must be finite and positive");
            assert(
                isfinite(g_f_) && g_f_ >= g_t_ && "smooth_gain_t: g_f must be finite and greater than or equal to g_t");

            auto const v_50_delta = v_50_ - v_0_;
            assert(isfinite(v_50_delta) && "smooth_gain_t: v_50 - v_0 must be finite");

            log_v_50_delta_ = log(v_50_delta);
            log_g_t_ = log(g_t_);
            log_gain_delta_ = log(g_f_) - log_g_t_;
        }

        template <typename value_t> [[nodiscard]] auto operator()(value_t input) const noexcept -> value_t
        {
            using std::exp;

            assert(primal(input) >= scalar_t{0} && "smooth_gain_t: input must be nonnegative");
            if (primal(input) <= v_0_) return value_t{g_t_};

            auto const transition = transition_value(input);
            return exp(log_g_t_ + log_gain_delta_ * transition);
        }

        /// finite nonnegative input coordinate
        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {scalar_t{0}, std::numeric_limits<scalar_t>::max()};
        }

        /// exact lower transition boundary
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {v_0_}; }

    private:
        template <typename value_t> [[nodiscard]] auto transition_value(value_t input) const noexcept -> value_t
        {
            using std::exp;
            using std::log;

            // H = z^k/(1 + z^k) = logistic(k*log(z)). Evaluate the logistic by sign so large or tiny z cannot
            // overflow an intermediate even though H itself is always finite and bounded.
            auto const logit = k_ * (log(input - v_0_) - log_v_50_delta_);
            if (primal(logit) >= scalar_t{0})
            {
                auto const tail = exp(-logit);
                return value_t{1} / (value_t{1} + tail);
            }

            auto const head = exp(logit);
            return head / (value_t{1} + head);
        }

        scalar_t v_0_;
        scalar_t v_50_;
        scalar_t k_;
        scalar_t g_t_;
        scalar_t g_f_;
        scalar_t log_v_50_delta_;
        scalar_t log_g_t_;
        scalar_t log_gain_delta_;
    };

    struct config_t
    {
        using curve_t = smooth_gain_t;

        reflection::param_t<float_t, reflection::constraints::static_t<float_t, -1e3, 1e3>> v_0{"v_0", 0.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, -1e3, 1e3>> v_50{"v_50", 5.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 2.0, 1e3>> k{"k", 3.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> g_t{"g_t", 2.0 / 3.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> g_f{"g_f", 3.0 / 2.0};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            self.v_0.reflect(inspector);
            self.v_50.reflect(inspector);
            self.k.reflect(inspector);
            self.g_t.reflect(inspector);
            self.g_f.reflect(inspector);
            return std::forward<inspector_t>(inspector);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };
};

/// converts from frontend config to implementation params
///
/// Smooth gain only has one parameterization, so this is a passthrough.
template <std::floating_point real_t>
constexpr auto to_params(smooth_gain_t::config_t const& config) -> smooth_gain_t::params_t<real_t>
{
    return {
        .v_0 = static_cast<real_t>(config.v_0.value()),
        .v_50 = static_cast<real_t>(config.v_50.value()),
        .k = static_cast<real_t>(config.k.value()),
        .g_t = static_cast<real_t>(config.g_t.value()),
        .g_f = static_cast<real_t>(config.g_f.value()),
    };
}

} // namespace crv::model::curves
