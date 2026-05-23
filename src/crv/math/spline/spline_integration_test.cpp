// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/test/test.hpp>

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
#include <crv/math/spline/construction/segment/amr/approximant.hpp>
#include <crv/math/spline/construction/segment/amr/bisection.hpp>
#include <crv/math/spline/construction/segment/amr/error_metric.hpp>
#include <crv/math/spline/construction/segment/amr/function_sampler.hpp>
#include <crv/math/spline/construction/segment/amr/interval.hpp>
#include <crv/math/spline/construction/segment/amr/node_generator.hpp>
#include <crv/math/spline/construction/segment/amr/residual_estimator.hpp>
#include <crv/math/spline/construction/segment/amr/subdivision.hpp>
#include <crv/math/spline/construction/segment/amr/subdivision_predicate.hpp>
#include <crv/math/spline/construction/segment/field_packer.hpp>
#include <crv/math/spline/construction/segment/quantization/mantissa_quantizer.hpp>
#include <crv/math/spline/construction/segment/quantization/radix_aligner.hpp>
#include <crv/math/spline/construction/segment/quantization/segment_quantizer.hpp>
#include <crv/math/spline/construction/segment/quantization/shift_planner.hpp>
#include <crv/math/spline/construction/segment/segment_factory.hpp>
#include <crv/math/spline/construction/segment/segment_packer.hpp>
#include <crv/math/spline/construction/spline/amr/seed/critical_point_conditioner.hpp>
#include <crv/math/spline/construction/spline/amr/seed/dyadic_stride_calculator.hpp>
#include <crv/math/spline/construction/spline/amr/seed/span_decomposer.hpp>
#include <crv/math/spline/construction/spline/amr/seed/subdomain_factory.hpp>
#include <crv/math/spline/construction/spline/amr/typestates.hpp>
#include <crv/math/spline/construction/spline/amr/workspace.hpp>
#include <crv/math/spline/construction/spline/tangent_extension.hpp>
#include <crv/math/spline/construction/weight_functions/hyperbolic_decay.hpp>
#include <crv/math/spline/pipeline_config.hpp>
#include <crv/math/spline/segment.hpp>
#include <crv/math/spline/segment_locator.hpp>
#include <crv/math/spline/spline.hpp>
#include <crv/priority_queue.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <stdfloat>
#include <vector>

namespace crv {
namespace spline {
namespace {

/// seeds queue with initial set of segments
///
/// This type splits the mapped domain using pure bisection to subdivide at a set of critical points.
template <typename typestate_t, typename span_decomposer_t, int_t log2_domain_max> struct refinement_pool_seeder_t
{
    using x_t = span_decomposer_t::x_t;
    using scalar_t = span_decomposer_t::scalar_t;
    using jet_t = span_decomposer_t::jet_t;
    using function_sample_t = span_decomposer_t::function_sample_t;

    [[no_unique_address]] span_decomposer_t decompose_span;

    auto operator()(typestate_t state, auto const& sample_target_function,
        std::vector<x_t> const& critical_points) const -> typename typestate_t::next_t
    {
        auto& workspace = state.workspace;
        auto& refinement_pool = workspace.refinement_pool;
        assert(refinement_pool.empty());

        // start at 0
        auto left_critical_point = x_t{0};
        auto left_function_sample
            = sample_target_function(jet_t{from_fixed<scalar_t>(left_critical_point), scalar_t{1}});

        // proceed through pairs of critical points
        for (auto const& right_critical_point : critical_points)
        {
            left_function_sample = decompose_span(sample_target_function, left_function_sample, left_critical_point,
                right_critical_point, refinement_pool);
            left_critical_point = right_critical_point;
        }

        // finish with end of domain
        auto const domain_end = x_t{1 << log2_domain_max};
        decompose_span(sample_target_function, left_function_sample, left_critical_point, domain_end, refinement_pool);

        return typename typestate_t::next_t{workspace};
    }
};

template <typename typestate_t, typename subdivider_t, typename subdivision_predicate_t, int_t max_segment_count>
struct refiner_t
{
    using interval_t = subdivider_t::interval_t;

    subdivision_predicate_t requires_subdivision;
    subdivider_t subdivide;

    auto operator()(typestate_t&& state, auto const& sample_target_function) -> typename typestate_t::next_t
    {
        auto& workspace = state.workspace;
        auto& refinement_pool = workspace.refinement_pool;
        auto& completed_intervals = workspace.completed_intervals;
        assert(!refinement_pool.empty());
        assert(completed_intervals.empty());

        // subdivide until full
        while (!refinement_pool.empty() && refinement_pool.size() + completed_intervals.size() < max_segment_count)
        {
            // this uses a *reference*; pop must be very specifically placed
            auto const& interval = refinement_pool.top();
            if (requires_subdivision(interval))
            {
                auto const children = subdivide(interval, sample_target_function);
                refinement_pool.pop();
                refinement_pool.push(children.left);
                refinement_pool.push(children.right);
            }
            else
            {
                completed_intervals.push_back(interval);
                refinement_pool.pop();
            }
        }

        // drain remaining
        while (!refinement_pool.empty())
        {
            completed_intervals.push_back(refinement_pool.top());
            refinement_pool.pop();
        }

        return typename typestate_t::next_t{workspace};
    }
};

template <typename typestate_t, typename interval_t, typename t_tangent_extender_t, int_t domain_max> struct assembler_t
{
    using tangent_extender_t = t_tangent_extender_t;

    [[no_unique_address]] tangent_extender_t extend_tangent;

    template <typename spline_t> auto operator()(typestate_t&& state, spline_t& spline) const -> void
    {
        auto& workspace = state.workspace;
        auto& completed_intervals = workspace.completed_intervals;
        assert(!completed_intervals.empty());

        std::ranges::sort(completed_intervals, std::ranges::less{},
            [](interval_t const& interval) noexcept { return interval.subdomain.left.x; });

        using segment_locator_t = spline_t::segment_locator_t;
        using segments_t = spline_t::segments_t;
        using segment_t = interval_t::segment_t;
        using x_t = segment_t::x_t;

        auto const segment_count = int_cast<int_t>(std::size(completed_intervals));
        assert(segment_locator_t::max_segment_count >= segment_count);
        static_assert(segment_locator_t::total_key_count + 1 == spline_t::max_segment_count);

        segments_t segments;
        using sorted_keys_t = std::array<x_t, segment_locator_t::total_key_count>;
        sorted_keys_t sorted_keys;

        segments[0] = completed_intervals[0].segment;
        for (auto segment_index = 1; segment_index < segment_count; ++segment_index)
        {
            auto const& interval = completed_intervals[segment_index];
            segments[segment_index] = interval.segment;
            sorted_keys[segment_index - 1] = to_fixed<x_t>(interval.subdomain.left.x);
        }

        // pad keys
        auto const x_max = x_t{domain_max};
        for (auto key_padding_index = segment_count; key_padding_index < segment_locator_t::total_key_count;
            ++key_padding_index)
        {
            sorted_keys[key_padding_index] = x_max;
        }

        static_assert(std::same_as<typename segments_t::value_type, typename spline_t::segment_t>);
        static_assert(std::same_as<segment_locator_t, typename spline_t::segment_locator_t>);
        static_assert(std::same_as<typename segment_locator_t::x_t, x_t>);

        // extend final tangent
        auto const final_interval = completed_intervals[segment_count - 1];
        auto const extended_tangent = extend_tangent(final_interval);

        completed_intervals.clear();

        auto const segment_locator = segment_locator_t{sorted_keys, x_max, segment_count};
        spline = spline_t{segment_locator, segments, extended_tangent};
    }
};

template <std::floating_point scalar_t, typename x_t, typename spline_t, typename typestates_t,
    typename refinement_pool_t, typename refinement_pool_seeder_t, typename refiner_t, typename assembler_t,
    int_t max_segment_count>
class spline_generator_t
{
public:
    constexpr spline_generator_t(refinement_pool_seeder_t seed_refinement_pool, refiner_t refine, assembler_t assemble)
        : seed_refinement_pool_{std::move(seed_refinement_pool)}, refine_{std::move(refine)},
          assemble_{std::move(assemble)}, workspace_{}
    {}

    constexpr auto operator()(auto& spline, auto target_function, std::vector<x_t> critical_points = {}) -> void
    {
        assert(workspace_.empty());
        workspace_.clear();

        auto sample_target_function = function_sampler_t{std::move(target_function)};

        auto unseeded_state = typename typestates_t::initial_t{workspace_};
        auto unrefined_state
            = seed_refinement_pool_(std::move(unseeded_state), sample_target_function, critical_points);
        auto unassembled_state = refine_(std::move(unrefined_state), sample_target_function);
        assemble_(std::move(unassembled_state), spline);

        assert(workspace_.empty());
    }

private:
    using interval_t = refiner_t::interval_t;
    using residual_t = interval_t::residual_t;

    using workspace_t = typestates_t::workspace_t;

    refinement_pool_seeder_t seed_refinement_pool_;
    refiner_t refine_;
    assembler_t assemble_;
    workspace_t workspace_;
};

TEST(spline_generator, poc)
{
#if 0
    using scalar_t = std::float128_t;
#else
    using scalar_t = float_t;
#endif

    using traits_t = traits_t<unpacked_field_t<int_t>>;
    using mantissa_t = traits_t::mantissa_t;
    using unpacked_field_t = traits_t::unpacked_field_t;
    using packed_field_t = traits_t::packed_field_t;
    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using packed_segment_t = traits_t::packed_segment_t;

    using x_t = prod_pipeline_config_t::x_t;
    using y_t = prod_pipeline_config_t::y_t;

    constexpr auto depth_max = 4;
    constexpr auto max_segment_count = 1 << (depth_max * 2);
    constexpr auto log2_domain_max = 8;
    constexpr auto domain_max = 1 << log2_domain_max;
    constexpr auto log2_min_width = -10;
    constexpr auto global_tolerance = 1e-10; // should max against integral

    constexpr auto segment_layout = prod_pipeline_config.segment_layout;
    constexpr auto intermediate_layout_max_shift = segment_layout.intermediate.max_shift();
    constexpr auto final_layout_min_shift = segment_layout.final.min_shift();
    constexpr auto final_layout_max_shift = segment_layout.final.max_shift();

    using cubic_t = cubic_t<scalar_t>;

    using error_norm_t = error_metric_t;
    auto const error_norm = error_norm_t{};

#if 1
    using weight_function_t = weight_functions::hyperbolic_decay_t<scalar_t>;
    auto const weight_function = weight_function_t{.halflife = 0.5};
#else
    using weight_function_t = weight_functions::uniform_t<scalar_t>;
    auto const weight_function = weight_function_t{};
#endif

    using segment_evaluator_t = segment_evaluator_t<traits_t, x_t, y_t>;
    using field_unpacker_t = field_unpacker_t<unpacked_field_t>;
    using segment_unpacker_t
        = segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
    using segment_t = segment_t<traits_t, x_t, segment_unpacker_t, segment_evaluator_t>;
    using subdomain_t = subdomain_t<scalar_t>;
    using interval_t = interval_t<subdomain_t, cubic_t, segment_t>;
    using refinement_pool_t = priority_queue_t<std::vector<interval_t>, interval_priority_less_t>;
    using node_generator_t = node_generator_t<scalar_t, 8>;
    using residual_estimator_t = residual_estimator_t<scalar_t, node_generator_t, error_norm_t, weight_function_t>;
    using hermite_converter_t = hermite_converter_t<scalar_t>;
    using approximant_t = approximant_t<scalar_t, segment_t>;
    using approximant_factory_t = approximant_factory_t<approximant_t>;
    using float_extractor_t = float_extractor_t<scalar_t>;
    using exponent_aligner_t = exponent_aligner_t<final_layout_min_shift, final_layout_max_shift>;
    using scaled_int_t = float_extractor_t::scaled_int_t;
    using radix_aligner_t = radix_aligner_t<unpacked_field_t, scaled_int_t, exponent_aligner_t{}>;
    using field_packer_t = field_packer_t<packed_field_t>;
    using mantissa_quantizer_t = mantissa_quantizer_t<mantissa_t>;
    using segment_quantizer_t
        = segment_quantizer_t<unpacked_field_t, float_extractor_t, shift_planner_t, mantissa_quantizer_t,
            radix_aligner_t, intermediate_layout_max_shift, x_t::frac_bits, y_t::frac_bits, log2_min_width>;
    using segment_packer_t = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>;
    using segment_factory_t = segment_factory_t<segment_t, segment_quantizer_t, segment_packer_t>;
    using interval_factory_t = interval_factory_t<interval_t, segment_factory_t, approximant_factory_t,
        hermite_converter_t, residual_estimator_t>;
    using bisection_t = bisection_t<subdomain_t>;
    using bisector_t = bisector_t<bisection_t>;
    using subdivision_predicate_t = subdivision_predicate_t<scalar_t, log2_min_width>;
    using subdivision_t = subdivision_t<interval_t>;
    using subdivider_t = subdivider_t<subdivision_t, bisector_t, interval_factory_t>;
    using segment_locator_t = segment_locator_t<x_t, depth_max>;
    using workspace_t = workspace_t<interval_t, interval_priority_less_t, max_segment_count>;
    using typestates_t = typestates_t<workspace_t>;
    using extended_tangent_t = extended_tangent_t<x_t, y_t, unpacked_field_t>;
    using tangent_extender_t = tangent_extender_t<interval_t, extended_tangent_t, float_extractor_t>;
    using assembler_t = assembler_t<typestates_t::unassembled_t, interval_t, tangent_extender_t, domain_max>;
    using refiner_t = refiner_t<typestates_t::unrefined_t, subdivider_t, subdivision_predicate_t, max_segment_count>;
    using dyadic_stride_calculator_t = seed::dyadic_stride_calculator_t<x_t>;
    using subdomain_factory_t = seed::subdomain_factory_t<x_t, subdomain_t>;
    using span_decomposer_t = seed::span_decomposer_t<dyadic_stride_calculator_t, subdomain_factory_t,
        interval_factory_t, max_segment_count, log2_min_width>;
    using refinement_pool_seeder_t
        = refinement_pool_seeder_t<typestates_t::unseeded_t, span_decomposer_t, log2_domain_max>;
    using spline_t = spline_t<segment_t, extended_tangent_t, segment_locator_t>;
    using spline_generator_t = spline_generator_t<scalar_t, x_t, spline_t, typestates_t, refinement_pool_t,
        refinement_pool_seeder_t, refiner_t, assembler_t, max_segment_count>;

    auto const estimate_residual = residual_estimator_t{
        .generate_nodes = {},
        .measure_error = error_norm,
        .apply_weight = weight_function,
    };

    auto const create_interval = interval_factory_t{
        .segment_factory = {},
        .approximant_factory = {},
        .convert_hermite = {},
        .estimate_residual = estimate_residual,
    };

    auto generate_spline = spline_generator_t
    {
        refinement_pool_seeder_t{
            .decompose_span{.calculate_stride = {}, .create_subdomain = {}, .create_interval = create_interval},
        },
        refiner_t
        {
            .requires_subdivision = subdivision_predicate_t{.global_tolerance = global_tolerance},
            .subdivide = subdivider_t
            {
                .bisect = bisector_t{},
                .create_interval = create_interval,
            },
        },
        assembler_t{}
    };

    auto const target_function = [](auto x) static noexcept -> decltype(x) {
        using std::log1p;
        return 2.1 * log1p(x);
    };

    auto const condition_critical_points = seed::critical_point_conditioner_t<x_t, log2_min_width>{};

    auto spline = spline_t{};
    generate_spline(spline, std::ref(target_function),
        condition_critical_points({x_t{1 << 3}, x_t{1 << 5}, to_fixed<x_t>(248.973)}));

#if 1
    auto x_fixed = x_t{0};
    auto const sample_count = 255;
    auto const dx = x_t{domain_max} / sample_count;
    for (auto sample = 0; sample < sample_count; ++sample, x_fixed += dx)
    {
        auto const x_real = from_fixed<scalar_t>(x_fixed);

        auto const expected_y = target_function(x_real);
        auto const actual_y = from_fixed<scalar_t>(spline(x_fixed));

        auto const difference = actual_y - expected_y;

        ASSERT_LT(abs(difference), 8.0e-7);

        std::cout << std::setprecision(4) << "x = " << static_cast<float_max_t>(x_real)
                  << ", f(x) = " << static_cast<float_max_t>(expected_y)
                  << ", y_actual = " << static_cast<float_max_t>(actual_y)
                  << ", Δy = " << static_cast<float_max_t>(difference) << std::endl;
    }
    std::cout << std::endl;
#endif
}

} // namespace
} // namespace spline
} // namespace crv
