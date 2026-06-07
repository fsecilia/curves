// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/curves/derivatives.hpp>
#include <crv/curves/traits.hpp>
#include <crv/math/complex.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/reflection/constraints.hpp>
#include <crv/reflection/param.hpp>
#include <complex>
#include <numbers>
#include <vector>

namespace crv::model::curves {

/// log-normal curve
///
/// This curve is the CDF of a log-normal:
///
///     f(x) = 1/2 + 1/2 erf((log x - mu)/(sigma*sqrt2))
///
/// Mu and sigma are derived from the center and width of the transition from 0. f rises monotonically from a saturated
/// floor of 0 and approaches 1.0 asymptotically.
struct log_normal_t
{
    //
    // config
    //

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

    //
    // evaluator
    //

    template <typename t_scalar_t> class evaluator_t
    {
    public:
        using scalar_t = t_scalar_t;

        using real_t = real_type_t<scalar_t>;
        using jet_t = crv::jet_t<scalar_t>;

        static constexpr double x_origin_saturation_threshold = 1e-12;

        constexpr explicit evaluator_t(config_t const& config) noexcept
            : mu_{log(static_cast<scalar_t>(config.center.value())) + static_cast<scalar_t>(config.width.value())},
              c_{scalar_t{1} / (sqrt(static_cast<scalar_t>(config.width.value())) * sqrt2_)}
        {}

        template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
        {
            using std::real;

            auto const x = primal(input);

            // origin branch
            if (real(x) < x_origin_saturation_threshold) return value_t{};

            // linear in s with dz/ds = c constant.
            auto const z = (log(x) - mu_) * c_;
            auto const f = scalar_t{0.5} + scalar_t{0.5} * complex_step_erf(z);

            if constexpr (is_jet<value_t>)
            {
                auto const f_s1 = c_ * inv_sqrt_pi_ * exp(-(z * z));
                auto const inv_x = scalar_t{1} / x;
                auto const d1 = f_s1 * inv_x;
                return {f, d1 * tangent(input)};
            }
            else return f;
        }

        /// array of critical points
        ///
        /// The log-normal CDF is strictly monotone in x, so f' > 0 for all finite x > 0, so there are no critical
        /// points.
        auto critical_points() const noexcept -> std::vector<scalar_t> { return {}; }

    private:
        static constexpr real_t sqrt2_ = std::numbers::sqrt2_v<real_t>;
        static constexpr real_t inv_sqrt_pi_ = std::numbers::inv_sqrtpi_v<real_t>;

        scalar_t mu_; // log(center) + width
        scalar_t c_; // dz/ds = 1/(sigma*sqrt2), sigma = sqrt(width)
    };
};

} // namespace crv::model::curves
