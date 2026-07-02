// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>
#include <crv/overloaded.hpp>
#include <crv/quadrature/adaptive_integrator.hpp>
#include <crv/quadrature/antiderivative.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <crv/shaping/transforms/input_affine.hpp>
#include <crv/shaping/transforms/output_affine.hpp>
#include <crv/shaping/transitions/smootherstep_integral.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cassert>
#include <concepts>
#include <iomanip>
#include <iostream>
#include <variant>

namespace crv {
namespace {

using real_t = float_t;

namespace shaping::transforms {

// stub; the real limiter is multiple modules
template <std::floating_point real_t> struct limit_transform_t
{
    real_t limit_val;
    constexpr auto operator()(real_t x) const noexcept -> real_t { return std::min(x, limit_val); }
};

} // namespace shaping::transforms

// ============================================================================
// PIPELINE ARCHITECTURE
// ============================================================================
namespace shaping::transforms {

/// Policy 1: Domain Warp (Formerly offset_t)
template <std::floating_point real_t, typename transition_t> class input_offset_warp_t
{
public:
    constexpr input_offset_warp_t(real_t start, real_t width, transition_t transition) noexcept
        : start_{start}, width_{width}, transition_{std::move(transition)}
    {
        rwidth_ = width_ > real_t{0} ? real_t{1} / width_ : real_t{0};
        lag_ = start_ + width_ * (real_t{1} - primal(transition_(real_t{1})));
    }

    [[nodiscard]] constexpr auto operator()(real_t input) const noexcept -> real_t
    {
        if (width_ == real_t{0}) return input;
        if (input <= start_) return real_t{0};
        if (input >= start_ + width_) return input - lag_;

        auto const t = (input - start_) * rwidth_;
        return width_ * primal(transition_(t));
    }

private:
    real_t start_{};
    real_t width_{};
    real_t rwidth_{};
    real_t lag_{};
    [[no_unique_address]] transition_t transition_{};
};

/// Policy 2: Derivative Blend
template <std::floating_point real_t, typename transition_t, typename antiderivative_t> class output_derivative_blend_t
{
public:
    constexpr output_derivative_blend_t(
        real_t start, real_t width, real_t flat_output, transition_t transition, antiderivative_t cache) noexcept
        : start_{start}, width_{width}, rwidth_{width > real_t{0} ? real_t{1} / width : real_t{0}},
          flat_output_{flat_output}, transition_{std::move(transition)}, cache_{std::move(cache)}
    {
        if (width_ > real_t{0}) { total_deficit_ = cache_(width_); }
    }

    [[nodiscard]] constexpr auto operator()(real_t y_base, real_t u) const noexcept -> real_t
    {
        if (width_ == real_t{0}) return y_base;
        if (u <= start_) return flat_output_;
        if (u >= start_ + width_) return flat_output_ + y_base - total_deficit_;

        auto const local_u = u - start_;
        auto const t = local_u * rwidth_;
        auto const partial_deficit = cache_(local_u);

        return flat_output_ + (primal(transition_(t)) * y_base) - partial_deficit;
    }

private:
    real_t start_{};
    real_t width_{};
    real_t rwidth_{};
    real_t flat_output_{};
    real_t total_deficit_{};
    [[no_unique_address]] transition_t transition_{};
    antiderivative_t cache_;
};

/// The Monolithic Pipeline
template <typename real_t, typename curve_t, typename warp_t, typename blend_t> struct curve_pipeline_t
{
    static_assert(std::is_invocable_r_v<real_t, curve_t, real_t>);

    crv::shaping::transforms::input_affine_t<real_t> input_affine;

    // Strict mutual exclusion via std::variant
    using offset_policy_t = std::variant<std::monostate, warp_t, blend_t>;
    offset_policy_t offset_policy;

    curve_t base_curve;
    crv::shaping::transforms::output_affine_t<real_t> output_affine;
    limit_transform_t<real_t> limiter;

    [[nodiscard]] constexpr auto operator()(real_t const& x) const -> real_t
    {
        real_t const u = input_affine(x);

        // Branchless equivalent routing using overloaded idiom
        real_t const y_out = std::visit(crv::overloaded_t{[&](std::monostate) { return base_curve(u); },
                                            [&](warp_t const& warp) { return base_curve(warp(u)); },
                                            [&](blend_t const& blend) {
                                                real_t const y_base = base_curve(u);
                                                return blend(y_base, u);
                                            }},
            offset_policy);

        return limiter(output_affine(y_out));
    }
};

} // namespace shaping::transforms

// ============================================================================
// CONFIGURATION & FACTORY
// ============================================================================
namespace shaping::construction {

/// Bridges the pipeline derivative blend to the adaptive quadrature engine
template <std::floating_point real_t, typename curve_t, typename transition_t> struct blend_integrand_t
{
    curve_t const* curve;
    transition_t transition;
    real_t start;
    real_t rwidth;

    // Fulfills the standard `invocable` concept required by the rule/integrator
    constexpr auto operator()(real_t local_u) const noexcept -> real_t
    {
        auto const u = start + local_u;
        auto const t = local_u * rwidth;

        // Extract accurate derivative via dual numbers (jets)
        auto const trans_jet = transition(jet_t<real_t>{t, real_t{1}});
        return (*curve)(u)*tangent(trans_jet) * rwidth;
    }
};

/// Type resolver to ensure both factory methods return the identical pipeline type
template <std::floating_point real_t, typename curve_t, typename transition_t,
    typename rule_t = quadrature::rules::gauss_kronrod_t<real_t>>
struct pipeline_traits
{
    using integrand_t = blend_integrand_t<real_t, curve_t, transition_t>;
    using integral_t = quadrature::integral_t<integrand_t, rule_t>;
    using cache_t = quadrature::antiderivative_of_t<integral_t>;

    using warp_t = transforms::input_offset_warp_t<real_t, transition_t>;
    using blend_t = transforms::output_derivative_blend_t<real_t, transition_t, cache_t>;
    using pipeline_t = transforms::curve_pipeline_t<real_t, curve_t, warp_t, blend_t>;
};

template <std::floating_point real_t, typename integrator_t> class pipeline_factory_t
{
public:
    constexpr pipeline_factory_t(integrator_t integrator) noexcept : integrator_{std::move(integrator)} {}

    template <typename curve_t, typename transition_t>
    constexpr auto build_warped(curve_t curve, real_t start, real_t width, transition_t transition,
        crv::shaping::transforms::input_affine_t<real_t> in_affine,
        crv::shaping::transforms::output_affine_t<real_t> out_affine,
        transforms::limit_transform_t<real_t> limiter) const
    {
        using traits = pipeline_traits<real_t, curve_t, transition_t>;

        return typename traits::pipeline_t{
            in_affine, typename traits::warp_t{start, width, transition}, std::move(curve), out_affine, limiter};
    }

    template <typename curve_t, typename transition_t>
    constexpr auto build_blended(curve_t curve, real_t start, real_t width, real_t flat_output, transition_t transition,
        crv::shaping::transforms::input_affine_t<real_t> in_affine,
        crv::shaping::transforms::output_affine_t<real_t> out_affine,
        transforms::limit_transform_t<real_t> limiter) const
    {
        using traits = pipeline_traits<real_t, curve_t, transition_t>;

        auto const rwidth = width > real_t{0} ? real_t{1} / width : real_t{0};
        typename traits::integrand_t integrand{&curve, transition, start, rwidth};
        typename traits::integral_t integral{integrand, quadrature::rules::gauss_kronrod_t<real_t>{}};

        auto const empty_critical_points = std::array<real_t, 0>{};
        auto result = integrator_(integral, width, empty_critical_points);

        return typename traits::pipeline_t{in_affine,
            typename traits::blend_t{start, width, flat_output, transition, std::move(result.antiderivative)},
            std::move(curve), out_affine, limiter};
    }

private:
    integrator_t integrator_;
};

} // namespace shaping::construction

// ============================================================================
// SMOKE TEST / MAIN
// ============================================================================

struct linear_curve_t
{
    constexpr auto operator()(real_t x) const noexcept -> real_t { return x * 2.0; }
};

// smoke test, just to make everything is referenced and builds
TEST(shaping_integration_test, smoke_test)
{
    using namespace crv::shaping;
    using transition_t = transitions::smootherstep_integral_t;

    auto integrator = quadrature::adaptive_integrator_t<real_t>{1e-6, 32};

    shaping::construction::pipeline_factory_t<real_t, decltype(integrator)> factory{integrator};

    // Shared params
    crv::shaping::transforms::input_affine_t<real_t> in_affine{1.0, 0.0};
    crv::shaping::transforms::output_affine_t<real_t> out_affine{1.0, 0.0};
    shaping::transforms::limit_transform_t<real_t> limiter{10.0};

    auto const x_start = 1.0;
    auto const x_width = 2.0;
    auto const x_max = x_start + x_width;

    // 1. Build a Warped Pipeline
    auto warped_pipe
        = factory.build_warped(linear_curve_t{}, x_start, x_width, transition_t{}, in_affine, out_affine, limiter);

    // 2. Build a Blended Pipeline
    auto blended_pipe = factory.build_blended(
        linear_curve_t{}, x_start, x_width, 0.0, transition_t{}, in_affine, out_affine, limiter);

    std::cout << "--- Curve Pipeline PoC ---" << std::endl;
    std::cout << std::fixed << std::setprecision(3);

    std::cout << "\nWarped Pipeline Evaluations:" << std::endl;
    for (real_t x = 0.0; x <= x_max * 2; x += 1.0)
    {
        std::cout << "x = " << x << " -> y = " << warped_pipe(x) << std::endl;
    }

    std::cout << "\nBlended Pipeline Evaluations (uses integrator cache):" << std::endl;
    for (real_t x = 0.0; x <= x_max * 2; x += 1.0)
    {
        std::cout << "x = " << x << " -> y = " << blended_pipe(x) << std::endl;
    }

    std::cout << "\nSuccess! Monolithic struct size: " << sizeof(warped_pipe) << " bytes." << std::endl;
}

} // namespace
} // namespace crv
