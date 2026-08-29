// SPDX-License-Identifier: MIT

/// \file
/// \brief conditioned numerical views used to construct splines from authored curves
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/quadrature/construction/adaptive_integrator.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <crv/ranges.hpp>
#include <array>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv::spline {

/// interpretation applied before spline construction
///
/// Dispatch uses this enum once, then gain and sensitivity continue as separate static target types.
enum class curve_construction_t
{
    gain,
    sensitivity,
};

/// conditioned target for a gain-authored curve
///
/// A finite gain can still have a cusp or singular derivative at the origin. Interpolating transfer
/// T(x) = x G(x) removes that problem, so Hermite construction works in transfer space.
template <typename t_curve_t> struct gain_curve_target_t
{
    using curve_t = t_curve_t;

    curve_t curve;

    template <typename value_t> constexpr auto gain(value_t x) const noexcept -> value_t { return curve(x); }

    template <std::floating_point scalar_t> constexpr auto transfer(scalar_t x) const noexcept -> scalar_t
    {
        return x * curve(x);
    }

    template <std::floating_point scalar_t> constexpr auto transfer(jet_t<scalar_t> x) const noexcept -> jet_t<scalar_t>
    {
        auto const primal_x = primal(x);

        // use limit definition to avoid evaluating gain derivative at origin
        //
        // A gain such as x^alpha, 0 < alpha < 1, has finite f(0) but singular f'(0). Using the product rule there
        // would create 0 * inf -> NaN even though transfer is regular and T'(0) = f(0).
        if (primal_x == scalar_t{0}) return {scalar_t{0}, curve(primal_x) * tangent(x)};

        return x * curve(x);
    }
};

template <typename curve_t> gain_curve_target_t(curve_t) -> gain_curve_target_t<curve_t>;

/// conditioned target for a sensitivity-authored curve
///
/// Sensitivity f gives transfer T(x) = integral_0^x f(t) dt and gain G(x) = T(x) / x. Near zero, dividing a prefix
/// integral by x would magnify its absolute error. mean_integrand() evaluates the same gain as a mean instead and
/// reuses the adaptive prefix cache. Transfer is rebuilt as x * G(x), while its tangent is f(x).
template <typename t_antiderivative_t> struct sensitivity_curve_target_t
{
    using antiderivative_t = t_antiderivative_t;
    using scalar_t = antiderivative_t::scalar_t;
    using jet_t = crv::jet_t<scalar_t>;

    antiderivative_t antiderivative;

    constexpr auto gain(scalar_t x) const noexcept -> scalar_t { return antiderivative.mean_integrand(x); }
    constexpr auto transfer(scalar_t x) const noexcept -> scalar_t { return x * gain(x); }
    constexpr auto transfer(jet_t x) const noexcept -> jet_t
    {
        auto const primal_x = primal(x);
        auto const effective_gain = gain(primal_x);
        return {primal_x * effective_gain, antiderivative.derivative(primal_x) * tangent(x)};
    }
};

template <typename antiderivative_t>
sensitivity_curve_target_t(antiderivative_t) -> sensitivity_curve_target_t<antiderivative_t>;

/// converts gain error tolerance to quadrature area tolerance
///
/// Quadrature splits the global area tolerance by interval width and preserves that split during bisection. For domain
/// width X, epsilon_T = X * epsilon_G keeps prefix error near a * epsilon_G for a prefix of length a. Dividing by a
/// leaves the cached mean on the requested gain-error scale.
template <std::floating_point scalar_t>
constexpr auto gain_tolerance_to_integral_tolerance(scalar_t domain_width, scalar_t gain_tolerance) noexcept -> scalar_t
{
    assert(domain_width >= scalar_t{0});
    assert(gain_tolerance >= scalar_t{0});
    return domain_width * gain_tolerance;
}

/// sensitivity-target construction result
///
/// Hitting a refinement limit does not discard the target. The receipt records whether every interval reached the
/// requested tolerance or noise floor.
template <typename t_target_t> struct sensitivity_curve_target_result_t
{
    using target_t = t_target_t;
    using scalar_t = target_t::scalar_t;

    target_t target;
    scalar_t achieved_error;
    scalar_t max_error;
    bool refinement_limited = false;
};

/// constructs a sensitivity target by adaptively integrating the authored sensitivity curve
template <std::floating_point t_scalar_t> struct sensitivity_curve_target_builder_t
{
    using scalar_t = t_scalar_t;
    using rule_t = quadrature::rules::gauss_kronrod_t<scalar_t>;

    scalar_t gain_tolerance;
    int_t depth_limit;

    template <typename curve_t> auto operator()(curve_t curve, scalar_t domain_end) const
    {
        return operator()(std::move(curve), domain_end, std::array<scalar_t, 0>{});
    }

    template <typename curve_t>
    auto operator()(curve_t curve, scalar_t domain_end, compatible_range<scalar_t> auto const& critical_points) const
    {
        assert(domain_end > scalar_t{0});
        auto const integral_tolerance = gain_tolerance_to_integral_tolerance(domain_end, gain_tolerance);
        auto integrate = quadrature::construction::adaptive_integrator_t<scalar_t>{integral_tolerance, depth_limit};
        auto result = integrate(quadrature::integral_t{std::move(curve), rule_t{}}, domain_end, critical_points);

        auto target = sensitivity_curve_target_t{std::move(result.antiderivative)};
        return sensitivity_curve_target_result_t<decltype(target)>{
            .target = std::move(target),
            .achieved_error = result.receipt.achieved_error,
            .max_error = result.receipt.max_error,
            .refinement_limited = result.receipt.refinement_limited,
        };
    }
};

} // namespace crv::spline
