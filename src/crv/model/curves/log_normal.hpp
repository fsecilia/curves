// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/complex.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/lambert.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/model/curves/traits.hpp>
#include <crv/model/domain.hpp>
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
/// f rises monotonically from a saturated floor of 0 and approaches a limit of 1.0 asymptotically. The evaluator
/// applies an affine transform to offset the baseline and scale the limit. The final curve output is:
///
///     delta = limit - baseline
///     z = (log(x) - mu)/(sigma*sqrt2)
///     f(x)  = baseline + delta*(1/2 + 1/2*erf(z)),
///     f'(x) = delta*exp(-z^2)/(x*sigma*sqrt(2*pi))
///
struct log_normal_t
{
    //
    // implementation params
    //

    template <std::floating_point real_t> struct params_t
    {
        using curve_t = log_normal_t;

        real_t baseline;
        real_t limit;
        real_t mu;
        real_t sigma;
    };

    //
    // evaluator
    //

    template <typename t_scalar_t> class evaluator_t
    {
    public:
        using curve_t = log_normal_t;
        using scalar_t = t_scalar_t;
        using real_t = real_type_t<scalar_t>;
        using domain_t = model::unbounded_domain_t<real_t>;

        using jet_t = crv::jet_t<scalar_t>;

        static constexpr auto x_origin_saturation_threshold = real_t{1e-12};

        constexpr explicit evaluator_t(params_t<real_t> const& params) noexcept
            : baseline_{params.baseline}, scale_{params.limit - params.baseline}, mu_{params.mu},
              dz_ds{scalar_t{1} / (params.sigma * sqrt2_)}
        {}

        template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
        {
            using std::exp;
            using std::real;

            auto const x = primal(input);

            // origin branch
            if (real(x) < x_origin_saturation_threshold) return value_t{baseline_};

            // linear in dz_dx
            auto const z = (log(x) - mu_) * dz_ds;
            auto const f = scalar_t{0.5} + scalar_t{0.5} * complex_step_erf(z);

            // affine transform
            auto const scaled_f = scale_ * f + baseline_;

            if constexpr (is_jet<value_t>)
            {
                auto const f_s1 = scale_ * dz_ds * rsqrt_pi_ * exp(-(z * z));
                auto const inv_x = scalar_t{1} / x;
                auto const d1 = f_s1 * inv_x;
                return {scaled_f, d1 * tangent(input)};
            }
            else return value_t{scaled_f};
        }

        /// scalar evaluation is saturated and finite for every finite real input
        [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }

        /// no interior critical points
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }

    private:
        static constexpr auto sqrt2_ = std::numbers::sqrt2_v<real_t>;
        static constexpr auto rsqrt_pi_ = std::numbers::inv_sqrtpi_v<real_t>;

        scalar_t baseline_;
        scalar_t scale_;
        scalar_t mu_; // log(center) + width
        scalar_t dz_ds; // dz/ds = 1/(sigma*sqrt2), sigma = sqrt(width)
    };

    //
    // frontend config
    //

    struct config_t
    {
        using curve_t = log_normal_t;

        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 0.0, 1e3>> baseline{
            "baseline", 2.0 / 3.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 0.0, 1e3>> limit{"limit", 1.5};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 0.0, 1e3>> accel_peak{
            "accel_peak", 5.0};
        reflection::param_t<float_t, reflection::constraints::static_t<float_t, 0.0, 1e3>> max_accel{"max_accel", 0.2};

        template <typename self_t, typename inspector_t>
        constexpr auto reflect(this self_t&& self, inspector_t&& inspector) -> decltype(auto)
        {
            self.baseline.reflect(inspector);
            self.limit.reflect(inspector);
            self.accel_peak.reflect(inspector);
            self.max_accel.reflect(inspector);

            return std::forward<inspector_t>(inspector);
        }

        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };
};

/// converts from frontend config to implementation params
///
/// This function solves (mu, sigma) from (acceleration_peak, maximum_acceleration):
///
///     argmax f' = exp(mu - sigma^2) => mu = log(acceleration_peak) + sigma^2
///     f'(acceleration_peak) = delta*exp(-sigma^2/2)/(acceleration_peak*sigma*sqrt(2*pi)) = maximum_acceleration
///
/// With u = log(x), f' maximizes -(u - mu)^2/(2*sigma^2) - u, giving u* = mu - sigma^2.
template <std::floating_point real_t>
constexpr auto to_params(log_normal_t::config_t const& config) -> log_normal_t::params_t<real_t>
{
    using std::numbers::pi_v;

    auto const acceleration_peak = config.accel_peak.value();
    auto const maximum_acceleration = config.max_accel.value();
    auto const baseline = config.baseline.value();
    auto const limit = config.limit.value();

    real_t const scale = limit - baseline;
    real_t const normalized_slope
        = (maximum_acceleration * acceleration_peak * std::sqrt(real_t{2} * pi_v<real_t>)) / scale;
    real_t const lambert_input = real_t{1} / (normalized_slope * normalized_slope);

    real_t const normal_variance = lambert_w0(lambert_input);

    return {
        .baseline = baseline,
        .limit = limit,
        .mu = std::log(acceleration_peak) + normal_variance,
        .sigma = std::sqrt(normal_variance),
    };
}

} // namespace crv::model::curves
