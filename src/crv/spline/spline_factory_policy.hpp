// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/fixed/quantizer.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/priority_queue.hpp>
#include <crv/spline/construction/segment/amr/approximant.hpp>
#include <crv/spline/construction/segment/amr/bisection.hpp>
#include <crv/spline/construction/segment/amr/error_metric.hpp>
#include <crv/spline/construction/segment/amr/interval.hpp>
#include <crv/spline/construction/segment/amr/node_generator.hpp>
#include <crv/spline/construction/segment/amr/residual_estimator.hpp>
#include <crv/spline/construction/segment/amr/subdivision.hpp>
#include <crv/spline/construction/segment/amr/subdivision_predicate.hpp>
#include <crv/spline/construction/segment/field_packer.hpp>
#include <crv/spline/construction/segment/local_coordinate.hpp>
#include <crv/spline/construction/segment/segment_factory.hpp>
#include <crv/spline/construction/segment/segment_packer.hpp>
#include <crv/spline/construction/segment/segment_quantizer.hpp>
#include <crv/spline/construction/segment/shift_planner.hpp>
#include <crv/spline/construction/spline/amr/assembler.hpp>
#include <crv/spline/construction/spline/amr/refinement_pool_seeder.hpp>
#include <crv/spline/construction/spline/amr/refiner.hpp>
#include <crv/spline/construction/spline/amr/seed/subdomain_factory.hpp>
#include <crv/spline/construction/spline/amr/spline_generator.hpp>
#include <crv/spline/construction/spline/amr/typestates.hpp>
#include <crv/spline/construction/spline/amr/workspace.hpp>
#include <crv/spline/construction/spline/tangent_extender.hpp>
#include <crv/spline/construction/weight_functions/hyperbolic_decay.hpp>
#include <crv/spline/construction/weight_functions/uniform.hpp>
#include <crv/spline/segment.hpp>
#include <crv/spline/segment_locator.hpp>
#include <crv/spline/spline.hpp>
#include <crv/spline/tangent_extension.hpp>
#include <concepts>
#include <type_traits>

namespace crv::spline {

template <std::floating_point t_scalar_t, typename t_pipeline_config_t> struct default_spline_policy_t
{
    using scalar_t = t_scalar_t;
    using pipeline_config_t = t_pipeline_config_t;
    using x_t = pipeline_config_t::x_t;
    using y_t = pipeline_config_t::y_t;

    // bounds and constraints
    static constexpr auto depth_max = 4;
    static constexpr auto log2_domain_end = 8;
    static constexpr auto log2_min_width = -10;
    static constexpr auto y_limit = scalar_t{1000.0};
    static constexpr auto max_segment_count = 1 << (depth_max * 2);
    static constexpr auto domain_end = 1 << log2_domain_end;

    // layout configuration
    static constexpr auto segment_layout = pipeline_config_t::segment_layout;
    static constexpr auto intermediate_layout_max_shift = segment_layout.intermediate.max_shift();
    static constexpr auto final_layout_min_shift = segment_layout.final.min_shift();
    static constexpr auto final_layout_max_shift = segment_layout.final.max_shift();

    // fundamental traits
    using unpacked_field_t = unpacked_field_t<int_t>;
    using traits_t = traits_t<unpacked_field_t, y_t>;
    using mantissa_t = traits_t::mantissa_t;
    using packed_field_t = traits_t::packed_field_t;
    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using packed_segment_t = traits_t::packed_segment_t;

    // math components
    using cubic_t = crv::cubic_t<scalar_t>;
    using error_norm_t = error_metric_t;
    using weight_function_t = weight_functions::hyperbolic_decay_t<scalar_t>;

    // segment pipeline
    using segment_evaluator_t = crv::spline::segment_evaluator_t<traits_t, x_t, y_t>;
    using field_unpacker_t = crv::spline::field_unpacker_t<unpacked_field_t>;
    using segment_unpacker_t
        = crv::spline::segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
    using segment_t = crv::spline::segment_t<traits_t, x_t, segment_unpacker_t, segment_evaluator_t>;
    static_assert(sizeof(packed_segment_t) == 32);
    static_assert(sizeof(segment_t) == 32);
    static_assert(alignof(segment_t) == 32);
    static_assert(std::is_trivially_copyable_v<packed_segment_t>);
    static_assert(std::is_trivially_copyable_v<segment_t>);
    using subdomain_t = crv::spline::subdomain_t<scalar_t, x_t>;
    using interval_t = crv::spline::interval_t<subdomain_t, cubic_t, segment_t>;

    // quantization and packing
    using float_extractor_t = float_extractor_t<scalar_t>;
    using exponent_aligner_t = exponent_aligner_t<final_layout_min_shift, final_layout_max_shift>;
    using scaled_int_t = float_extractor_t::scaled_int_t;
    using radix_aligner_t = crv::spline::radix_aligner_t<unpacked_field_t, scaled_int_t, exponent_aligner_t{}>;
    using field_packer_t = crv::spline::field_packer_t<packed_field_t>;
    using mantissa_quantizer_t = crv::spline::mantissa_quantizer_t<mantissa_t>;
    using shift_planner_t = crv::spline::shift_planner_t<mantissa_t>;
    using segment_quantizer_t = crv::spline::segment_quantizer_t<unpacked_segment_t, float_extractor_t, shift_planner_t,
        mantissa_quantizer_t, radix_aligner_t, intermediate_layout_max_shift, x_t>;
    using segment_packer_t
        = crv::spline::segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>;
    using segment_factory_t = crv::spline::segment_factory_t<segment_t, segment_quantizer_t, segment_packer_t>;

    // amr
    using refinement_pool_t = priority_queue_t<std::vector<interval_t>, crv::spline::interval_priority_less_t>;
    using node_generator_t = crv::spline::node_generator_t<scalar_t, 8>;
    using residual_estimator_t
        = crv::spline::residual_estimator_t<scalar_t, node_generator_t, error_norm_t, weight_function_t>;
    using hermite_converter_t = hermite_converter_t<scalar_t>;
    using approximant_t = crv::spline::approximant_t<scalar_t, segment_t>;
    using approximant_factory_t = crv::spline::approximant_factory_t<approximant_t>;
    using local_coordinate_converter_t = crv::spline::local_coordinate_converter_t<scalar_t>;
    using interval_factory_t = crv::spline::interval_factory_t<interval_t, segment_factory_t, approximant_factory_t,
        hermite_converter_t, local_coordinate_converter_t, residual_estimator_t>;
    using bisection_t = crv::spline::bisection_t<subdomain_t>;
    using bisector_t = crv::spline::bisector_t<bisection_t>;
    using subdivision_predicate_t = crv::spline::subdivision_predicate_t<scalar_t, x_t, log2_min_width>;
    using subdivision_t = crv::spline::subdivision_t<interval_t>;
    using subdivider_t = crv::spline::subdivider_t<subdivision_t, bisector_t, interval_factory_t>;

    // assembly
    using segment_locator_t = crv::spline::segment_locator_t<x_t, depth_max>;
    using workspace_t = crv::spline::workspace_t<interval_t, crv::spline::interval_priority_less_t, max_segment_count>;
    using typestates_t = crv::spline::typestates_t<workspace_t>;
    using extended_tangent_t = crv::spline::extended_tangent_t<x_t, y_t, unpacked_field_t>;
    using tangent_extender_t = crv::spline::tangent_extender_t<interval_t, extended_tangent_t, float_extractor_t>;

    // orchestrators
    using assembler_t
        = crv::spline::assembler_t<typename typestates_t::unassembled_t, interval_t, crv::spline::interval_sorter_t,
            crv::spline::interval_unzipper_t, crv::spline::key_padder_t, tangent_extender_t, domain_end>;
    using refiner_t = crv::spline::refiner_t<typename typestates_t::unrefined_t, subdivider_t, subdivision_predicate_t,
        max_segment_count>;
    using subdomain_factory_t = crv::spline::seed::subdomain_factory_t<x_t, subdomain_t>;
    using refinement_pool_seeder_t = crv::spline::refinement_pool_seeder_t<typename typestates_t::unseeded_t,
        subdomain_factory_t, interval_factory_t, max_segment_count, log2_domain_end>;

    // final target
    using spline_t = crv::spline::spline_t<segment_t, extended_tangent_t, segment_locator_t>;
    using spline_generator_t = crv::spline::spline_generator_t<scalar_t, x_t, spline_t, typestates_t, refinement_pool_t,
        refinement_pool_seeder_t, refiner_t, assembler_t>;
};

} // namespace crv::spline
