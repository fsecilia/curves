// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>
#include <crv/signal_chain/curves/derivatives.hpp>
#include <crv/signal_chain/curves/traits.hpp>
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
    /// model config
    struct config_t
    {
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1.0, 1e3>> motivity{"Motivity", 1.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> gamma{"Gamma", 1.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1.0 / 16, 1.0>> smooth{"Smooth", 0.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> sync_speed{
            "Sync Speed", 5.0};

        template <typename self_t, typename visitor_t>
        constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
        {
            self.motivity.reflect(visitor);
            self.gamma.reflect(visitor);
            self.smooth.reflect(visitor);
            self.sync_speed.reflect(visitor);

            return std::forward<visitor_t>(visitor);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };

    /// evaluator
    template <is_curve_scalar t_scalar_t> class evaluator_t
    {
    public:
        using scalar_t = t_scalar_t;

        using real_t = real_type_t<scalar_t>;
        using jet_t = crv::jet_t<scalar_t>;

        constexpr explicit evaluator_t(config_t const& config) noexcept
            : m_{static_cast<scalar_t>(config.motivity.value())}, p_{static_cast<scalar_t>(config.sync_speed.value())},
              g_{static_cast<scalar_t>(config.gamma.value())}, P_{log(p_)}, M_{std::log(m_)}, G_{g_ / M_},
              k_{scalar_t{0.5} / static_cast<scalar_t>(config.smooth.value())}, r_{scalar_t{1} / k_}
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

            using std::exp;
            using std::log;
            using std::pow;
            using std::real;
            using std::tanh;

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
                auto const w = tanh(uk);
                auto const sech2 = scalar_t{1} - w * w; // sech^2 = 1 - tanh^2
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
            else return f;
        }

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

        real_t u_cusp_threshold_ = calc_u_cusp_threshold();
        real_t x_origin_limit_threshold_ = calc_x_origin_limit_threshold();

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

        // find threshold near x=0 where tanh saturates to min
        constexpr auto calc_x_origin_limit_threshold() const noexcept -> real_t
        {
            using std::pow;
            using std::real;

            // saturation floor: tanh(z) becomes 1.0 in double precision at z >= ~19.1
            auto const u_sat = pow(real_t{19.1}, real(r_));
            auto const x_sat = real(p_) * exp(-u_sat / real(G_));

            // safety floor: prevents inv_x^3 from overflowing to inf
            // This is important when we start taking the 3rd derivative again.
            auto const x_safe = pow(min<real_t>(), real_t{1.0 / 3.0});

            // The final threshold is the higher of the two
            return std::max(x_sat, x_safe);
        }
    };
};

} // namespace crv::model::curves
