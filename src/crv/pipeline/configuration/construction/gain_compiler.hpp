// SPDX-License-Identifier: MIT

/// \file
/// \brief authored curve to production gain spline
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/model/config.hpp>
#include <crv/model/curves/traits.hpp>
#include <crv/pipeline.hpp>
#include <crv/spline/construction/spline/amr/generation_result.hpp>
#include <crv/tuple.hpp>
#include <algorithm>
#include <exception>
#include <expected>
#include <utility>
#include <variant>
#include <vector>

namespace crv::pipeline::configuration::construction {

template <std::floating_point t_scalar_t, is_fixed t_x_t> struct critical_point_builder_t
{
    using scalar_t = t_scalar_t;
    using x_t = t_x_t;

    struct result_t
    {
        std::vector<scalar_t> integration;
        std::vector<x_t> spline;
    };

    template <typename curve_t> auto operator()(curve_t const& curve, scalar_t domain_end) const -> result_t
    {
        auto integration = curve.critical_points();
        std::erase_if(integration, [&](auto value) { return !(scalar_t{0} < value && value < domain_end); });
        std::ranges::sort(integration);
        auto const integration_duplicates = std::ranges::unique(integration);
        integration.erase(integration_duplicates.begin(), integration_duplicates.end());

        auto spline = std::vector<x_t>{};
        spline.reserve(integration.size());
        auto const fixed_domain_end = to_fixed<x_t>(domain_end);
        for (auto const value : integration)
        {
            auto const fixed_value = to_fixed<x_t>(value);
            if (x_t{} < fixed_value && fixed_value < fixed_domain_end) spline.push_back(fixed_value);
        }
        std::ranges::sort(spline);
        auto const spline_duplicates = std::ranges::unique(spline);
        spline.erase(spline_duplicates.begin(), spline_duplicates.end());

        return {.integration = std::move(integration), .spline = std::move(spline)};
    }
};

template <std::floating_point scalar_t> struct sensitivity_refinement_error_t
{
    scalar_t achieved_error{};
    scalar_t max_error{};

    constexpr auto operator==(sensitivity_refinement_error_t const&) const noexcept -> bool = default;
};

template <std::floating_point scalar_t, is_fixed x_t, typename shaping_error_t> struct gain_compilation_error_t
{
    using detail_t = std::variant<shaping_error_t, sensitivity_refinement_error_t<scalar_t>,
        spline::spline_generation_error_t<x_t>>;

    detail_t detail;

    constexpr auto operator==(gain_compilation_error_t const&) const noexcept -> bool = default;
};

template <typename t_spline_policy_t, typename t_shaped_curve_builder_t, typename t_critical_point_builder_t,
    typename t_gain_target_builder_t, typename t_sensitivity_target_builder_t, typename t_spline_factory_t>
struct gain_compiler_t
{
    using spline_policy_t = t_spline_policy_t;
    using shaped_curve_builder_t = t_shaped_curve_builder_t;
    using critical_point_builder_t = t_critical_point_builder_t;
    using gain_target_builder_t = t_gain_target_builder_t;
    using sensitivity_target_builder_t = t_sensitivity_target_builder_t;
    using spline_factory_t = t_spline_factory_t;

    using scalar_t = spline_policy_t::scalar_t;
    using x_t = spline_policy_t::x_t;
    using shaping_error_t = shaped_curve_builder_t::error_t;
    using error_t = gain_compilation_error_t<scalar_t, x_t, shaping_error_t>;
    using result_t = std::expected<void, error_t>;

    static_assert(std::same_as<typename spline_policy_t::spline_t, pipeline_t::gain_t>);

    [[no_unique_address]] shaped_curve_builder_t shape_curve;
    [[no_unique_address]] critical_point_builder_t build_critical_points;
    [[no_unique_address]] gain_target_builder_t build_gain_target;
    [[no_unique_address]] sensitivity_target_builder_t build_sensitivity_target;
    [[no_unique_address]] spline_factory_t build_spline;

    // compiles the authored curve into the runtime gain spline
    auto operator()(pipeline_t::gain_t& gain, model::curves_t const& curves) const -> result_t
    {
        auto const curve_index = static_cast<std::size_t>(curves.active.value());
        auto result = result_t{};
        tuple::visit_at(
            curves.configs, curve_index, [&](auto const& curve_config) { result = compile_curve(gain, curve_config); });
        return result;
    }

private:
    template <typename curve_config_t>
    auto compile_curve(pipeline_t::gain_t& gain, curve_config_t const& curve_config) const -> result_t
    {
        using config_t = curve_config_t::specific_curve_config_t;
        using curve_t = config_t::curve_t;
        using evaluator_t = curve_t::template evaluator_t<scalar_t>;

        auto const domain_end = scalar_t{spline_policy_t::domain_end};
        auto evaluator = evaluator_t{model::curves::to_params<scalar_t>(curve_config.specific)};
        auto shaped_result = shape_curve(std::move(evaluator), curve_config.common, domain_end);
        if (!shaped_result) return std::unexpected{error_t{.detail = shaped_result.error()}};
        auto curve = std::move(*shaped_result);
        auto critical_points = build_critical_points(curve, domain_end);

        switch (curve_config.interpretation.value())
        {
            case model::curve_interpretation_t::gain:
                return compile_gain_curve(gain, std::move(curve), std::move(critical_points.spline));
            case model::curve_interpretation_t::sensitivity:
                return compile_sensitivity_curve(gain, std::move(curve), domain_end, std::move(critical_points));
        }

        std::terminate();
    }

    template <typename curve_t>
    auto compile_gain_curve(pipeline_t::gain_t& gain, curve_t curve, std::vector<x_t> spline_points) const -> result_t
    {
        return compile_target(gain, build_gain_target(std::move(curve)), std::move(spline_points));
    }

    template <typename curve_t, typename critical_points_t>
    auto compile_sensitivity_curve(pipeline_t::gain_t& gain, curve_t curve, scalar_t domain_end,
        critical_points_t critical_points) const -> result_t
    {
        auto target = build_sensitivity_target(std::move(curve), domain_end, critical_points.integration);
        if (target.refinement_limited)
        {
            return std::unexpected{error_t{.detail = sensitivity_refinement_error_t<scalar_t>{
                                               .achieved_error = target.achieved_error,
                                               .max_error = target.max_error,
                                           }}};
        }

        return compile_target(gain, std::move(target.target), std::move(critical_points.spline));
    }

    template <typename target_t>
    auto compile_target(pipeline_t::gain_t& gain, target_t target, std::vector<x_t> spline_points) const -> result_t
    {
        auto const spline_result
            = build_spline(gain, std::move(target), spline_policy_t::spline_gain_tolerance, std::move(spline_points));
        if (!spline_result) return std::unexpected{error_t{.detail = *spline_result.error}};
        return {};
    }
};

} // namespace crv::pipeline::configuration::construction
