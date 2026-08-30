// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/model/curves/traits.hpp>
#include <crv/model/domain.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>
#include <complex>
#include <vector>

namespace crv::model::curves {

/// synchronous curve
///
/// This is RawAccel's popular Synchronous curve. It is an elegant realization of Steven's power law:
///
///     ψ(I) = kI^a
///
/// The main content of the curve fits the law very precisely, centered on x=p=sync_speed, y=1.0. It is composed with a
/// log-log sigmoid to smoothly saturate to y in [1/motivity, motivity].
///
/// This curve operates entirely on velocity ratios rather than absolute velocity differences:
///
///     u = G(log x - log p) = G(log (x/p))
///
/// A 2x increase in physical hand speed below p provokes the same geometric response as a 2x decrease above it. It
/// mirrors how the human nervous system perceives stimulus scaling.
///
/// The transition through p has a constant elasticity of response (f''(p) = g(g-1)/p^2).
///
/// A Desmos graph is provided here: https://www.desmos.com/calculator/viiczscidh
struct synchronous_t
{
    //
    // implementation params
    //

    template <std::floating_point real_t> struct params_t
    {
        using curve_t = synchronous_t;

        float_t motivity;
        float_t gamma;
        float_t smooth;
        float_t sync_speed;

        constexpr auto operator==(params_t const&) const noexcept -> bool = default;
    };

    //
    // evaluator
    //

    /// evaluator
    template <is_curve_scalar t_scalar_t> class evaluator_t
    {
    public:
        using curve_t = synchronous_t;
        using scalar_t = t_scalar_t;
        using real_t = real_type_t<scalar_t>;
        using domain_t = model::unbounded_domain_t<real_t>;

        using jet_t = crv::jet_t<scalar_t>;

        constexpr explicit evaluator_t(params_t<real_t> const& params) noexcept
            : m_{static_cast<scalar_t>(params.motivity)}, p_{static_cast<scalar_t>(params.sync_speed)},
              g_{static_cast<scalar_t>(params.gamma)}, P_{log(p_)}, M_{std::log(m_)},
              G_{params.motivity == real_t{1} ? scalar_t{} : g_ / M_},
              k_{scalar_t{0.5} / static_cast<scalar_t>(params.smooth)}, r_{scalar_t{1} / k_},
              unit_motivity_{params.motivity == real_t{1}},
              u_cusp_threshold_{unit_motivity_ ? real_t{} : calc_u_cusp_threshold()}
        {}

        template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
        {
            // This implementation differs from the original slightly in form but not content. It supports jets, but
            // calculates derivatives manually to reuse terms from base evaluation.
            //
            // The calculation is kept in log space with s = log x, where the inner argument, u = G*(s - log p), is
            // linear in s, so du/ds = G is constant and differentiating in s never produces a 1/x. 1/x only appears in
            // the final chain rule conversion to x-space.
            //
            // There are 3 branches:
            //     origin: x < x_origin_limit_threshold_, f = 1/m, all derivatives 0
            //     cusp: |u| < threshold_u_, f = (x/p)^g
            //     elsewhere: f = exp(sign*M*w^r), w = tanh(|u|^k)
            //
            // Some valid parameterizations have a vertical asymptote at 0. The origin branch,
            // x < x_origin_limit_threshold_, isolates the one place an origin asymptote can bite. This region sidesteps
            // it by using the limit defintion, returning the saturated value with zero slope.
            //
            // There is a cusp in the first derivative at x = p -> u = 0. In this vicinity, |u| < threshold_u_, the
            // local form is exactly (x/p)^g. This form is used over the smallest window required.
            //
            // Elsewhere, it is the original f(x).
            //
            // This function supports complex jets to test derivatives via infintesimal complex steps rather than finite
            // differences. Each branch interior is holomorphic, as are both sides of the cusp independently. Branch
            // selection uses the real part, so tests must stay within a single branch interior, away from the cusp, the
            // origin, and their analytical special cases.
            //
            // The base function is f(x) = m^(sgn(log(x/p))*(tanh((g/log(m))*|log(x/p)|)^(0.5/r))^(r/0.5))
            //
            //     Origin Branch (x < x_origin_limit_threshold_):
            //
            //         f(x)  = 1/m
            //         f'(x) = 0
            //
            //     Cusp Branch (|u| <= threshold_u_):
            //
            //         f(x)  = (x/p)^g
            //         f'(x) = f(x)*g/x
            //
            //     Main Branch (Elsewhere):
            //
            //         u = G*(log(x) - log(p))
            //         w = tanh(|u|^k)
            //         P = w^(r-1)
            //         E = sgn(u)*M*w^r
            //
            //         E' = sgn(u)*M*r*P*w'
            //         f_s1 = f(x)*E'
            //
            //         f(x)  = exp(E)
            //         f'(x) = f_s1/x
            //

            using std::cosh;
            using std::exp;
            using std::log;
            using std::pow;
            using std::real;
            using std::tanh;

            if (unit_motivity_) return value_t{scalar_t{1}};

            auto const x = primal(input);

            // origin branch
            if (real(x) < x_origin_limit_threshold_)
            {
                // use limit definition: f = 1/m, every derivative 0
                return value_t{scalar_t{1} / m_};
            }

            auto const s = log(x);
            auto const s_minus_logp = s - P_;
            auto const u = G_ * s_minus_logp;

            // value f, and (for order >= 1) the log-space derivatives f_s1, built cumulatively
            auto f = scalar_t{};
            auto f_s1 = scalar_t{};

            // cusp branch
            if (abs(real(u)) <= u_cusp_threshold_)
            {
                // use power-law equivalent: log f = g*(s - log p)
                //
                // log f is linear in s, so the log-space derivatives are trivial and smooth:
                //
                //     f_s1 = g f
                //
                f = exp(g_ * s_minus_logp); // (x/p)^g
                if constexpr (is_jet<value_t>) f_s1 = g_ * f;
            }
            else
            {
                // f = exp(sign*M*w^r), w = tanh(|u|^k), du/ds = G, a constant.
                auto const sgn = real(u) < real_t{0.0} ? real_t{-1.0} : real_t{1.0};
                auto const u_abs = real(u) < real_t{0.0} ? -u : u; // scalar abs based on sign of real
                auto const a = pow(u_abs, k_ - scalar_t{1}); // |u|^(k-1)
                auto const uk = a * u_abs; // |u|^k

                // sech^2(x) = 4/(e^x + e^-x)^2 = 4e^-2x/(1+e^-2x)^2
                // uk > 0; negative uk overflows e^-2uk
                assert(real(uk) > real_t{0});
                auto const e_m2uk = exp(scalar_t{-2} * uk);
                auto const sech_denom = scalar_t{1} + e_m2uk;
                auto const sech_denom2 = sech_denom * sech_denom;
                auto const sech2 = scalar_t{4} * e_m2uk / sech_denom2;

                auto const w = tanh(uk);
                auto const P = pow(w, r_ - scalar_t{1}); // w^(r-1)
                auto const w_r = P * w; // w^r

                f = exp(sgn * M_ * w_r); // sign rides the exponent, f<1 below the cusp, f>1 above

                // odd orders carry sgn; (sgn)^2 = 1 collapses it out of the even orders.
                auto const B = sgn * k_ * a * G_; // B = d(|u|^k)/ds = k*|u|^(k-1)*d|u|/ds, d|u|/ds=sgn*G.
                auto const w1 = sech2 * B; // w'
                auto const E1 = sgn * M_ * r_ * P * w1; // E'
                f_s1 = f * E1; // f_s1 = f E'
            }

            // convert log-space -> x-space:
            if constexpr (is_jet<value_t>)
            {
                // f' = f_s1/x
                auto const d1 = f_s1 / x;
                return {f, d1 * tangent(input)};
            }
            else return value_t{f};
        }

        /// scalar evaluation is saturated and finite for every finite real input
        [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }

        /// array of critical points
        ///
        /// This curve has one critical point, at the cusp.
        auto critical_points() const -> std::vector<scalar_t> { return {p_}; }

    private:
        scalar_t m_; // motivity
        scalar_t p_; // sync_speed
        scalar_t g_; // gamma
        scalar_t P_; // log(sync_speed)
        scalar_t M_; // log(motivity)
        scalar_t G_; // gamma/log(motivity)
        scalar_t k_; // 0.5/smooth
        scalar_t r_; // smooth/0.5
        bool unit_motivity_;

    public:
        // find threshold near u=0 that is affected by the cusp in the first derivative there
        //
        // This works backwards from the taylor series truncation error through the chain rule to the find hardware
        // limit.
        constexpr auto calc_u_cusp_threshold() const noexcept -> real_t
        {
            using std::real;

            // w = u^k
            // Δw = u^(3*k)/3
            // f = exp(M*w^r), Δf ~= (df/dw)*Δw
            // df/dw = f*(M*r*w^(r-1))
            // lim[u -> 0] exp(M*w^r) -> 1
            // df/dw ~= M*r*(u^k)^(r - 1)
            // r = 1/k, k*(r - 1) = 1 - k
            // df/dw ~= M*r*u^(1 - k)
            // Δf ~= M*r*u^(1 - k)*(u^(3*k)/3)
            // 1 - k + 3*k = 1 + 2*k
            // Δf ~= (M*r/3)*u^(1 + 2*k)
            // u^(1 + 2*k) = 3e/(M*r)
            // u = (3e/(M*r))^(1/(1 + 2*k))
            auto const epsilon = std::numeric_limits<real_t>::epsilon();
            return pow(3.0 * epsilon / (real(M_) * real(r_)), real_t{1} / (1 + 2 * real(k_)));
        }

        // find threshold near x=0 where 3rd derivative overflows to inf
        //
        // This checks for 1/x^3 so the 3rd derivative doesn't overflow converting from log space back to linear. It
        // checks for the 3rd derivative because it is needed to accurately render thick lines of the 2nd derivative. We
        // only take the first derivative here because we are limited by the 1-jet. When n-jets are ready, we will be
        // taking up to the 3rd derivative to render the first two.
        static auto calc_x_origin_limit_threshold() noexcept -> real_t
        {
            auto const min = std::numeric_limits<real_t>::min();
            return std::cbrt(min);
        }

    private:
        real_t u_cusp_threshold_;
        inline static real_t x_origin_limit_threshold_ = calc_x_origin_limit_threshold();
    };

    //
    // frontend config
    //

    struct config_t
    {
        using curve_t = synchronous_t;

        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1.0, 1e3>> motivity{"motivity", 1.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> gamma{"gamma", 1.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1.0 / 16, 1.0>> smooth{"smooth", 0.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> sync_speed{
            "sync_speed", 5.0};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            self.motivity.reflect(inspector);
            self.gamma.reflect(inspector);
            self.smooth.reflect(inspector);
            self.sync_speed.reflect(inspector);

            return std::forward<inspector_t>(inspector);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };
};

/// converts from frontend config to implementation params
///
/// Synchronous only has one parameterization, so this is a passthrough.
template <std::floating_point real_t>
constexpr auto to_params(synchronous_t::config_t const& config) -> synchronous_t::params_t<real_t>
{
    return {
        .motivity = config.motivity.value(),
        .gamma = config.gamma.value(),
        .smooth = config.smooth.value(),
        .sync_speed = config.sync_speed.value(),
    };
}

} // namespace crv::model::curves
