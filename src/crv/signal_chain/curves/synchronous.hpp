// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/complex_traits.hpp>
#include <crv/math/jet/jet.hpp>
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
///     u = G*(log x - log p) = G(log (x/p))
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
              g_{static_cast<scalar_t>(config.gamma.value())},
              k_{scalar_t{0.5} / static_cast<scalar_t>(config.smooth.value())}, r_{scalar_t{1} / k_}
        {
            using std::log;
            L_ = log(m_);
            G_ = g_ / L_;

            threshold_u_ = calc_threshold_u();
            threshold_x_origin_ = calc_threshold_x_origin();
        }

        /// scalar form
        constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return derivatives<0>(x).f; }

        /// 1-jet form; honors input tangent
        constexpr auto operator()(jet_t x) const noexcept -> jet_t
        {
            auto const d = derivatives<1>(primal(x));
            return jet_t{d.f, d.d1 * tangent(x)};
        }

        /// array of critical points
        ///
        /// This curve has one critical point, at the cusp.
        auto critical_points() const -> std::vector<scalar_t> { return {p_}; }

        /// calculates base function and up to order-n derivatives
        ///
        /// This implementation differs from the original slightly in form but not content because multiple derivatives
        /// are needed when tracing in the ui.
        ///
        /// The calculation is kept in log space with s = log x, where the inner argument, u = G*(s - log p), is linear
        /// in s, so du/ds = G is constant and differentiating in s never produces a 1/x. 1/x only appears in the final
        /// chain-rule conversion to x-space.
        ///
        /// There are 3 branches:
        ///     origin: x < threshold_x_origin_, f = 1/m, all derivatives 0
        ///     cusp: |u| < threshold_u_, f = (x/p)^g
        ///     elsewhere: f = exp(sign*L*w^r), w = tanh(|u|^k)
        ///
        /// The origin branch, x < threshold_x_origin_, isolates the one place the origin asymptote can bite. This
        /// region sidesteps it by using the limit defintion, returning the saturated value with zero slope.
        ///
        /// There is a cusp in the first derivative at x = p -> u = 0. In this vicinity, |u| < threshold_u_, the local
        /// form is exactly (x/p)^g. This form is used over the smallest window required.
        ///
        /// Elsewhere, it is the original f(x).
        ///
        /// This function supports jets, but does the derivative calculation by hand. One function calculates all of the
        /// derivatives by increasing order, returning early when the requested order is reached. Common calculations
        /// are shared.
        ///
        /// This function supports complex numbers to test derivatives via infintesimal complex steps rather than finite
        /// differences. Each branch interior is holomorphic, as are both sides of the cusp independently. Branch
        /// selection uses the real part, so tests must stay within a single branch interior, away from the cusp, the
        /// origin, and their analytical special cases.
        ///
        /// This function is f(x) = m^(sgn(log(x/p))*(tanh((g/log(m))*|log(x/p)|)^(0.5/r))^(r/0.5))
        /// The 3rd derivative is a wall of algebra. Here are are the outputs, grouped by branch, in readable form:
        ///
        ///     Origin Branch (x < threshold_x_origin_):
        ///
        ///         f(x)    = 1/m
        ///         f'(x)   = 0
        ///         f''(x)  = 0
        ///         f'''(x) = 0
        ///
        ///     Cusp Branch (|u| <= threshold_u_):
        ///
        ///         f(x)    = (x/p)^g
        ///         f'(x)   = g * f / x
        ///         f''(x)  = g * (g - 1) * f / x^2
        ///         f'''(x) = g * (g - 1) * (g - 2) * f / x^3
        ///
        ///     Main Branch (Elsewhere):
        ///
        ///         s = log(x)
        ///         u = G * (s - log(p))
        ///         w = tanh(|u|^k)
        ///         P = w^(r-1)
        ///         a = |u|^(k-1)
        ///         E = sgn(u) * L * w^r
        ///
        ///         The exponent derivatives are:
        ///         E'   = sgn(u) * L * r * P * w'
        ///         E''  = sgn(u) * L * r * (P'w' + Pw'')
        ///         E''' = sgn(u) * L * r * (P''w' + 2P'w'' + Pw''')
        ///
        ///         The log-space derivatives (d/ds) are:
        ///         f_s1 = f(x) * E'
        ///         f_s2 = f(x) * (E'^2 + E'')
        ///         f_s3 = f(x) * (E'^3 + 3E'E'' + E''')
        ///
        ///         Giving final outputs:
        ///         f(x)    = exp(E)
        ///         f'(x)   = f_s1 / x
        ///         f''(x)  = (f_s2 - f_s1) / x^2
        ///         f'''(x) = (f_s3 - 3f_s2 + 2f_s) / x^3
        ///
        template <int_t order> constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
        {
            using std::exp;
            using std::log;
            using std::pow;
            using std::real;
            using std::tanh;

            static_assert(0 <= order && order <= 3);

            // origin branch
            if (real(x) < threshold_x_origin_)
            {
                // use limit definition: f = 1/m, every derivative 0
                auto result = derivatives_t<order, scalar_t>{};
                result.f = scalar_t{1} / m_;
                return result;
            }

            auto const s = log(x);
            auto const s_minus_logp = s - log(p_);
            auto const u = G_ * s_minus_logp;

            // value f, and (for order >= 1) the log-space derivatives f_s1, f_s2, f_s3, built cumulatively
            auto f = scalar_t{};
            auto f_s1 = scalar_t{};
            auto f_s2 = scalar_t{};
            auto f_s3 = scalar_t{};

            // cusp branch
            if (abs(real(u)) <= threshold_u_)
            {
                // use power-law equivalent: log f = g*(s - log p)
                //
                // log f is linear in s, so the log-space derivatives are trivial and smooth:
                //
                //     f_s1 = g f
                //     f_s2 = g^2 f
                //     f_s3 = g^3 f
                //
                f = exp(g_ * s_minus_logp); // (x/p)^g
                if constexpr (order >= 1) f_s1 = g_ * f;
                if constexpr (order >= 2) f_s2 = g_ * f_s1;
                if constexpr (order >= 3) f_s3 = g_ * f_s2;
            }
            else
            {
                // f = exp(sign*L*w^r), w = tanh(|u|^k). Everything bottoms out in du/ds = G, a constant.
                // Shared subexpressions, computed once:
                auto const sgn = real(u) < real_t{0.0} ? real_t{-1.0} : real_t{1.0};
                auto const u_abs = real(u) < real_t{0.0} ? -u : u; // scalar abs based on sign of real
                auto const a = pow(u_abs, k_ - scalar_t{1}); // |u|^(k-1)
                auto const uk = a * u_abs; // |u|^k
                auto const w = tanh(uk);
                auto const sech2 = scalar_t{1} - w * w; // sech^2 = 1 - tanh^2
                auto const P = pow(w, r_ - scalar_t{1}); // w^(r-1)
                auto const w_r = P * w; // w^r

                f = exp(sgn * L_ * w_r); // sign rides the exponent, f<1 below the cusp, f>1 above

                if constexpr (order >= 1)
                {
                    // odd orders carry sgn; (sgn)^2 = 1 collapses it out of the even orders.
                    auto const B = sgn * k_ * a * G_; // B = d(|u|^k)/ds = k*|u|^(k-1)*d|u|/ds, d|u|/ds=sgn*G.
                    auto const w1 = sech2 * B; // w'
                    auto const P1 = (r_ - scalar_t{1}) * P / w * w1; // P' = (r-1) P/w w'
                    auto const Lr = sgn * L_ * r_;
                    auto const E1 = Lr * P * w1; // E'
                    f_s1 = f * E1; // f_s1 = f E'

                    if constexpr (order >= 2)
                    {
                        // a' = (k-1)|u|^(k-2) d|u|/ds = (k-1) a/|u| (sgn G);
                        auto const a1 = (k_ - scalar_t{1}) * a / u_abs * (sgn * G_);
                        auto const B1 = sgn * k_ * G_ * a1; // B' = sgn*k*G*a'
                        auto const sech2_1 = scalar_t{-2} * w * w1; // (sech^2)'
                        auto const w2 = sech2_1 * B + sech2 * B1; // w''
                        auto const E2 = Lr * (P1 * w1 + P * w2); // E''
                        auto const E1_2 = E1 * E1;
                        f_s2 = f * (E1_2 + E2); // f_s2 = f (E'^2 + E'')

                        if constexpr (order >= 3)
                        {
                            // a'' = (k-1)(k-2)|u|^(k-3)(d|u|/ds)^2 = (k-1)(k-2) a/|u|^2 G^2;
                            auto const a2 = (k_ - scalar_t{1}) * (k_ - scalar_t{2}) * a / (u_abs * u_abs) * (G_ * G_);
                            auto const B2 = sgn * k_ * G_ * a2;
                            auto const sech2_2 = scalar_t{-2} * (w1 * w1 + w * w2); // (sech^2)''
                            auto const w3 = sech2_2 * B + scalar_t{2} * sech2_1 * B1 + sech2 * B2; // w'''
                            auto const P2 = (r_ - scalar_t{1}) * (P1 / w * w1 - P / (w * w) * w1 * w1 + P / w * w2);
                            auto const E3 = Lr * (P2 * w1 + scalar_t{2} * P1 * w2 + P * w3); // E'''
                            auto const E1_3 = E1_2 * E1;
                            f_s3 = f * (E1_3 + scalar_t{3} * E1 * E2 + E3); // f_s3 = f(E'^3 + 3E'E'' + E''')
                        }
                    }
                }
            }

            // convert log-space -> x-space:
            //
            //     f' = f_s1/x
            //     f'' = (f_s2 - f_s1)/x^2
            //     f''' = (f_s3 - 3f_s2 + 2f_s)/x^3
            //
            if constexpr (order == 0) return {f};
            else
            {
                auto const inv_x = scalar_t{1} / x;
                auto const d1 = f_s1 * inv_x;
                if constexpr (order == 1) { return {f, d1}; }
                else
                {
                    auto const inv_x2 = inv_x * inv_x;
                    auto const d2 = (f_s2 - f_s1) * inv_x2;
                    if constexpr (order == 2) { return {f, d1, d2}; }
                    else
                    {
                        auto const inv_x3 = inv_x2 * inv_x;
                        auto const d3 = (f_s3 - scalar_t{3} * f_s2 + scalar_t{2} * f_s1) * inv_x3;
                        return {f, d1, d2, d3};
                    }
                }
            }
        }

    private:
        // find threshold near u=0 that is affected by the cusp in the first derivative there
        //
        // This works backwards from the taylor series truncation error through the chain rule to the find hardware
        // limit.
        constexpr auto calc_threshold_u() const noexcept -> real_t
        {
            using std::real;

            // w = u^k
            // Δw = u^(3*k)/3
            // f = exp(L*w^r), Δf ~= (df/dw)*Δw
            // df/dw = f*(L*r*w^(r-1))
            // lim[u -> 0] exp(L*w^r) -> 1
            // df/dw ~= L*r*(u^k)^(r - 1)
            // r = 1/k, k*(r - 1) = 1 - k
            // df/dw ~= L*r*u^(1 - k)
            // Δf ~= L*r*u^(1 - k)*(u^(3*k)/3)
            // 1 - k + 3*k = 1 + 2*k
            // Δf ~= (L*r/3)*u^(1 + 2*k)
            // u^(1 + 2*k) = 3e/(L*r)
            // u = (3e/(L*r))^(1/(1 + 2*k))
            auto const epsilon = std::numeric_limits<real_t>::epsilon();
            return pow(3 * epsilon / (real(L_) * real(r_)), real_t{1} / (1 + 2 * real(k_)));
        }

        // find threshold near x=0 where tanh saturates to min
        constexpr auto calc_threshold_x_origin() const noexcept -> real_t
        {
            using std::pow;
            using std::real;

            // saturation floor: tanh(z) becomes 1.0 in double precision at z >= ~19.1
            auto const u_sat = pow(real_t{19.1}, real(r_));
            auto const x_sat = real(p_) * exp(-u_sat / real(G_));

            // safety floor: prevents inv_x^3 from overflowing to inf
            auto const x_safe = pow(min<real_t>(), real_t{1.0 / 3.0});

            // The final threshold is the higher of the two
            return std::max(x_sat, x_safe);
        }

        scalar_t m_; // motivity
        scalar_t p_; // sync_speed
        scalar_t g_; // gamma
        scalar_t L_; // log(motivity)
        scalar_t G_; // gamma/log(motivity)
        scalar_t k_; // 0.5/smooth
        scalar_t r_; // 1/k

        real_t threshold_u_;
        real_t threshold_x_origin_;
    };
};

} // namespace crv::model::curves
