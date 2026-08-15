// SPDX-License-Identifier: MIT

/// \file
/// \brief conditioned numerical views used to construct splines from authored curves
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/quadrature/adaptive_integrator.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <crv/ranges.hpp>
#include <array>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv::spline {

/// interpretation applied to an authored curve before spline construction
///
/// This enum belongs at the outer construction dispatch boundary. Once a branch has been selected, gain and
/// sensitivity targets remain distinct static types.
enum class curve_construction_t
{
    gain,
    sensitivity,
};

/// Conditioned target for a gain-authored curve.
///
/// Source curves are not necessarily suitable for direct Hermite interpolation. In particular, a finite continuous
/// gain may have a cusp or singular derivative at the origin. The transfer T(x) = x G(x) regularizes those cases and
/// is the quantity interpolated by Hermite construction.
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

        // Do not evaluate curve(jet{0,...}) here. A valid gain such as x^alpha, 0 < alpha < 1, has finite f(0) but
        // singular f'(0). Applying the product rule mechanically would manufacture 0 * inf -> NaN even though the
        // transfer is regular and T'(0) = f(0).
        if (primal_x == scalar_t{0}) return {scalar_t{0}, curve(primal_x) * tangent(x)};

        return x * curve(x);
    }
};

template <typename curve_t> gain_curve_target_t(curve_t) -> gain_curve_target_t<curve_t>;

/// Conditioned target for a sensitivity-authored curve.
///
/// Sensitivity f induces transfer T(x) = integral_0^x f(t) dt and effective gain G(x) = T(x) / x. The gain is not
/// evaluated by dividing an absolute-error prefix integral by x. antiderivative_t::mean_integrand() evaluates the
/// equivalent mean directly, G(x) = integral_0^1 f(xu) du, and reuses the adaptive prefix cache with a conditioned
/// convex blend. Transfer is then reconstructed as x * G(x), so its absolute numerical error naturally shrinks toward
/// the origin. Its tangent is the retained integrand directly by the Fundamental Theorem of Calculus.
template <typename t_antiderivative_t> struct sensitivity_curve_target_t
{
    using antiderivative_t = t_antiderivative_t;
    using scalar_t = typename antiderivative_t::scalar_t;
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

/// translates a requested sensitivity gain error into the absolute-area units used by adaptive quadrature
///
/// Adaptive seeding allocates the global area tolerance in proportion to interval width and bisection preserves that
/// allocation. With complete domain width X, choosing epsilon_T = X * epsilon_G therefore gives a prefix of length a
/// approximately epsilon_T(a) <= a * epsilon_G, keeping the cached prefix mean on the requested gain-error scale.
template <std::floating_point scalar_t>
constexpr auto gain_tolerance_to_integral_tolerance(scalar_t domain_width, scalar_t gain_tolerance) noexcept -> scalar_t
{
    assert(domain_width >= scalar_t{0});
    assert(gain_tolerance >= scalar_t{0});
    return domain_width * gain_tolerance;
}

/// sensitivity-target construction receipt
///
/// Quadrature remains best effort when a structural refinement limit is reached. The target is still usable, while
/// the receipt preserves whether the requested/noise-floor acceptance condition was not reached everywhere.
template <typename t_target_t> struct sensitivity_curve_target_result_t
{
    using target_t = t_target_t;
    using scalar_t = typename target_t::scalar_t;

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
        auto integrate = quadrature::adaptive_integrator_t<scalar_t>{integral_tolerance, depth_limit};
        auto result = integrate(quadrature::integral_t{std::move(curve), rule_t{}}, domain_end, critical_points);

        auto target = sensitivity_curve_target_t{std::move(result.antiderivative)};
        return sensitivity_curve_target_result_t<decltype(target)>{
            .target = std::move(target),
            .achieved_error = result.achieved_error,
            .max_error = result.max_error,
            .refinement_limited = result.refinement_limited,
        };
    }
};

} // namespace crv::spline
