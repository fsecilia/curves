// SPDX-License-Identifier: MIT

/// \file
/// \brief userspace compiler for authored runtime configuration
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/model/shaping/shaped_curve.hpp>
#include <crv/pipeline.hpp>
#include <crv/pipeline/configuration/construction/authored_validator.hpp>
#include <crv/pipeline/configuration/construction/compiler.hpp>
#include <crv/pipeline/configuration/construction/config_builder.hpp>
#include <crv/pipeline/configuration/construction/gain_compiler.hpp>
#include <crv/pipeline/configuration/construction/output_transform_builder.hpp>
#include <crv/quadrature/antiderivative_factory.hpp>
#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/construction/spline/amr/spline_generator.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>

namespace crv::pipeline::configuration {

class compiler_t
{
    using spline_policy_t = spline::default_spline_policy_t<float_t, spline::prod_pipeline_config_t>;
    using output_transform_builder_t
        = construction::output_transform_builder_t<decltype(pipeline_t::config_t{}.output_transform)>;
    using config_builder_t = construction::config_builder_t<
        construction::velocity_scale_builder_t, construction::half_life_builder_t, output_transform_builder_t>;
    using authored_validator_t = construction::authored_validator_t<construction::half_life_builder_t>;
    using critical_point_builder_t
        = construction::critical_point_builder_t<typename spline_policy_t::scalar_t, typename spline_policy_t::x_t>;
    using antiderivative_factory_t = quadrature::antiderivative_factory_t<spline_policy_t::scalar_t>;
    using sensitivity_target_builder_t = spline::sensitivity_curve_target_builder_t<antiderivative_factory_t>;
    using spline_factory_t
        = spline::spline_factory_t<spline_policy_t, spline::spline_generator_factory_t<spline_policy_t>>;
    using gain_compiler_t = construction::gain_compiler_t<spline_policy_t, shaping::shaped_curve_builder_t,
        critical_point_builder_t, sensitivity_target_builder_t, spline_factory_t>;
    using construction_compiler_t
        = construction::compiler_t<authored_validator_t, config_builder_t, gain_compiler_t, pipeline_t::validator_t>;

public:
    using error_t = construction_compiler_t::error_t;
    using result_t = construction_compiler_t::result_t;
    using validation_result_t = authored_validator_t::result_t;

    auto validate(model::device_t const& device, model::profile_t const& profile) const -> validation_result_t
    {
        return compiler_.validate_authored(device, profile);
    }

    auto operator()(model::device_t const& device, model::profile_t const& profile) const -> result_t
    {
        return compiler_(device, profile);
    }

private:
    construction_compiler_t compiler_{
        .validate_authored = authored_validator_t{.build_half_life = construction::half_life_builder_t{}},
        .build_config = config_builder_t{
            .build_velocity_scale = construction::velocity_scale_builder_t{},
            .build_half_life = construction::half_life_builder_t{},
            .build_output_transform = output_transform_builder_t{},
        },
        .compile_gain = gain_compiler_t{
            .shape_curve = shaping::shaped_curve_builder_t{},
            .build_critical_points = critical_point_builder_t{},
            .build_sensitivity_target = sensitivity_target_builder_t{
                .build_antiderivative = antiderivative_factory_t{},
                .gain_tolerance = spline_policy_t::sensitivity_gain_tolerance,
                .depth_limit = spline_policy_t::sensitivity_depth_limit,
            },
            .build_spline = spline_factory_t{.create_generator = spline::spline_generator_factory_t<spline_policy_t>{}},
        },
        .validate_runtime = pipeline_t::validator_t{},
    };
};

} // namespace crv::pipeline::configuration
