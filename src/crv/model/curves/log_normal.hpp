// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/curves/derivatives.hpp>
#include <crv/model/curves/traits.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>
#include <complex>
#include <concepts>
#include <numbers>
#include <vector>

namespace crv::model::curves {

/// log-normal curve
///
/// This curve is the CDF of a log-normal: f(x) = 1/2 + 1/2 erf((log x - mu)/(sigma*sqrt2)), with mu and sigma derived
/// from the center and width of the transition from 0. f rises monotonically from a saturated floor of 0 toward 1.
struct log_normal_t
{
    struct config_t
    {
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> center{"Center", 5.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 1e-3, 1e3>> width{"Width", 0.5};

        template <typename self_t, typename visitor_t>
        constexpr auto reflect(this self_t&& self, visitor_t&& visitor) -> decltype(auto)
        {
            self.center.reflect(visitor);
            self.width.reflect(visitor);

            return std::forward<visitor_t>(visitor);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };

    /// evaluator
    template <typename t_scalar_t> class evaluator_t
    {
    public:
        using scalar_t = t_scalar_t;

        using real_t = real_type_t<scalar_t>;
        using jet_t = crv::jet_t<scalar_t>;

        // x-space tolerance knob.
        static constexpr double threshold_x_origin = 1e-12; // below this x, treat as saturated origin

        constexpr explicit evaluator_t(config_t const& config) noexcept
            : mu_{log(static_cast<scalar_t>(config.center.value())) + static_cast<scalar_t>(config.width.value())},
              c_{scalar_t{1} / (sqrt(static_cast<scalar_t>(config.width.value())) * sqrt2_)}
        {}

        // scalar form
        constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return derivatives<0>(x).f; }

        /// 1-jet form; honors input tangent
        constexpr auto operator()(jet_t x) const noexcept -> jet_t
        {
            auto const d = derivatives<1>(primal(x));
            return jet_t{d.f, d.d1 * tangent(x)};
        }

        /// array of critical points
        ///
        /// The log-normal CDF is strictly monotone in x, so f' > 0 for all finite x > 0, so there are no critical
        /// points.
        auto critical_points() const -> std::vector<scalar_t> { return {}; }

        /// calculates base function and up to order-n derivatives
        template <int order> constexpr auto derivatives(scalar_t x) const noexcept -> derivatives_t<order, scalar_t>
        {
            using std::real;

            static_assert(order >= 0 && order <= 3, "lognormal exposes value and derivative orders 1..3");

            // origin branch
            if (real(x) < threshold_x_origin)
            {
                // use limit definition: f = 0, every derivative 0
                return derivatives_t<order, scalar_t>{};
            }

            // linear in s with dz/ds = c constant.
            auto const z = (log(x) - mu_) * c_; // z = (s - mu) c
            auto const f = scalar_t{0.5} + scalar_t{0.5} * complex_step_erf(z); // f = 1/2 + 1/2 erf(z)

            if constexpr (order == 0) return {f};
            else
            {
                // f_s1 = (c/sqrt(pi)) e^{-z^2}.
                auto const f_s1 = c_ * inv_sqrt_pi_ * exp(-(z * z));
                auto const inv_x = scalar_t{1} / x;
                auto const d1 = f_s1 * inv_x;
                if constexpr (order == 1) return {f, d1};
                else
                {
                    auto const f_s2 = f_s1 * (scalar_t{-2} * c_ * z); // d/ds of f_s1
                    auto const inv_x2 = inv_x * inv_x;
                    auto const d2 = (f_s2 - f_s1) * inv_x2;
                    if constexpr (order == 2) return {f, d1, d2};
                    else
                    {
                        auto const f_s3 = f_s1 * (c_ * c_) * (scalar_t{4} * z * z - scalar_t{2}); // d/ds of f_s2
                        auto const inv_x3 = inv_x2 * inv_x;
                        auto const d3 = (f_s3 - scalar_t{3} * f_s2 + scalar_t{2} * f_s1) * inv_x3;
                        return {f, d1, d2, d3};
                    }
                }
            }
        }

    private:
        // partial implementation of complex erf, just enough to run complex-step tests
        //
        // real scalar_t delegates to the standard-library erf. complex scalar_t is only implemented enough to support
        // complex-step via approximation. The input is z = a + i*b with |b| infinitesimal, so this returns the true
        // erf(a) in the real part and the exact analytic first-order tangent in the imaginary part:
        //
        //     b*erf'(a) = b*(2/sqrt(pi))*e^{-a^2}
        //
        // This is precisely what Im(f(x+ih))/h reads back. It is not erf off the real axis. For any finite b, the
        // imaginary part is wrong. It is correct solely for the infinitesimal b inputs complex-step produces, which is
        // the only way the complex evaluator is ever called.
        static auto complex_step_erf(scalar_t z) -> scalar_t
        {
            if constexpr (std::floating_point<scalar_t>) return erf(z);
            else
            {
                auto const a = z.real();
                auto const b = z.imag();
                return scalar_t{erf(a), b * two_over_sqrt_pi * exp(-a * a)};
            }
        }

        static constexpr real_t sqrt2_ = std::numbers::sqrt2_v<real_t>;
        static constexpr real_t inv_sqrt_pi_ = std::numbers::inv_sqrtpi_v<real_t>;
        static constexpr real_t two_over_sqrt_pi = 2.0 * std::numbers::inv_sqrtpi_v<real_t>;

        scalar_t mu_; // log(center) + width
        scalar_t c_; // dz/ds = 1/(sigma*sqrt2), sigma = sqrt(width)
    };
};

} // namespace crv::model::curves
