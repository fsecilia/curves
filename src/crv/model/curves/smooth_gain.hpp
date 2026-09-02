// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/model/curve_interpretation.hpp>
#include <crv/model/domain.hpp>
#include <crv/model/shaping/transitions/continuity.hpp>
#include <crv/model/shaping/transitions/nast.hpp>
#include <crv/model/shaping/transitions/smootherstep.hpp>
#include <crv/model/shaping/transitions/smootheststep.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <utility>
#include <vector>

namespace crv::model::curves {

/// smooth positive-gain transition in log space
///
///     u = (v - v_0)/(v_1 - v_0)
///     g(v) = exp(log(g_t) + (log(g_f) - log(g_t))*H(u))
///
/// The curve is exactly g_t at and below v_0 and exactly g_f at and above v_1. The blend is smooth gain in log space.
/// It should be run as a gain curve.
///
/// In the current build, H is selected at runtime from shaping::transitions. In the future, this will become part of
/// the type, but that is pending the cps builder.
/// H is selected at from the compact transition family.
struct smooth_gain_t
{
    static constexpr auto default_interpretation = curve_interpretation_t::gain;

    template <std::floating_point real_t> struct params_t
    {
        using curve_t = smooth_gain_t;

        real_t v_0;
        real_t v_1;
        real_t g_t;
        real_t g_f;
        shaping::transitions::continuity_t transition;

        constexpr auto operator==(params_t const&) const noexcept -> bool = default;
    };

    template <std::floating_point t_scalar_t> class evaluator_t
    {
    public:
        using curve_t = smooth_gain_t;
        using scalar_t = t_scalar_t;
        using jet_t = crv::jet_t<scalar_t>;

        explicit evaluator_t(params_t<scalar_t> const& params) noexcept
            : v_0_{params.v_0}, v_1_{params.v_1}, g_t_{params.g_t}, g_f_{params.g_f},
              inv_span_{scalar_t{1} / (params.v_1 - params.v_0)}, log_g_t_{std::log(params.g_t)},
              log_gain_delta_{std::log(params.g_f) - log_g_t_}, continuity_{params.transition}
        {
            assert(std::isfinite(v_0_) && "smooth_gain_t: v_0 must be finite");
            assert(std::isfinite(v_1_) && v_1_ > v_0_ && "smooth_gain_t: v_1 must be finite and greater than v_0");
            assert(std::isfinite(g_t_) && g_t_ > scalar_t{0} && "smooth_gain_t: g_t must be finite and positive");
            assert(std::isfinite(g_f_) && g_f_ > scalar_t{0} && "smooth_gain_t: g_f must be finite and positive");
        }

        template <typename value_t> [[nodiscard]] auto operator()(value_t input) const noexcept -> value_t
        {
            using std::exp;

            auto const x = primal(input);
            assert(x >= scalar_t{0} && "smooth_gain_t: input must be nonnegative");

            auto const u = (input - v_0_) * inv_span_;
            auto const u_value = primal(u);
            if (u_value <= scalar_t{0}) return value_t{g_t_};
            if (u_value >= scalar_t{1}) return value_t{g_f_};

            auto const transition = transition_value(u);
            return exp(log_g_t_ + log_gain_delta_ * transition);
        }

        /// finite nonnegative input coordinate
        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {scalar_t{0}, std::numeric_limits<scalar_t>::max()};
        }

        /// transition support boundaries
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {v_0_, v_1_}; }

    private:
        template <typename value_t> [[nodiscard]] auto transition_value(value_t u) const noexcept -> value_t
        {
            auto const u2 = u * u;
            return u2 * (((-scalar_t{4} * u + scalar_t{15}) * u - scalar_t{20}) * u + scalar_t{10});

#if 0
            using namespace shaping::transitions;

            // dispatches authored continuity as a value until shared cps curve builder can retain typed outputs
            switch (continuity_)
            {
                case continuity_t::c1: return smoothstep_t{}.value(u);
                case continuity_t::c2: return smootherstep_t{}.value(u);
                case continuity_t::c3: return smootheststep_t{}.value(u);
                case continuity_t::cinfinity: return nast_base_t<scalar_t>{}.value(u);
            }

            assert(false && "smooth_gain_t: continuity out of range");
            std::unreachable();
#endif
        }

        scalar_t v_0_;
        scalar_t v_1_;
        scalar_t g_t_;
        scalar_t g_f_;
        scalar_t inv_span_;
        scalar_t log_g_t_;
        scalar_t log_gain_delta_;
        shaping::transitions::continuity_t continuity_;
    };

    struct config_t
    {
        using curve_t = smooth_gain_t;

        reflection::param_t<float_t, reflection::constraints::static_t<float_t, -1e3, 1e3>> v_0{"v_0", 0.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, -1e3, 1e3>> v_1{"v_1", 10.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> g_t{"g_t", 2.0 / 3.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> g_f{"g_f", 3.0 / 2.0};
        reflection::param_t<shaping::transitions::continuity_t> transition{
            "transition", shaping::transitions::continuity_t::cinfinity};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            self.v_0.reflect(inspector);
            self.v_1.reflect(inspector);
            self.g_t.reflect(inspector);
            self.g_f.reflect(inspector);
            self.transition.reflect(inspector);
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
        .v_1 = static_cast<real_t>(config.v_1.value()),
        .g_t = static_cast<real_t>(config.g_t.value()),
        .g_f = static_cast<real_t>(config.g_f.value()),
        .transition = config.transition.value(),
    };
}

} // namespace crv::model::curves
