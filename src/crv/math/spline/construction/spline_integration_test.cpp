// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/test/test.hpp>

#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/construction/approximant.hpp>
#include <crv/math/spline/construction/defect_analyzer.hpp>
#include <crv/math/spline/construction/defect_checks/monotonicity.hpp>
#include <crv/math/spline/construction/defect_checks/overflow.hpp>
#include <crv/math/spline/construction/error_norm.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/hermite_converter.hpp>
#include <crv/math/spline/construction/node_generator.hpp>
#include <crv/math/spline/construction/quantizer.hpp>
#include <crv/math/spline/construction/residual_estimator.hpp>
#include <crv/math/spline/construction/segment_derivative.hpp>
#include <crv/math/spline/construction/segment_factory.hpp>
#include <crv/math/spline/construction/weight_function.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <crv/math/spline/segment.hpp>
#include <crv/priority_queue.hpp>
#include <algorithm>
#include <cmath>
#include <compare>
#include <expected>
#include <iomanip>
#include <numeric>
#include <variant>

namespace crv {
namespace spline {
namespace {

/// unit of work over subdomain
template <std::floating_point t_real_t, typename t_segment_t> struct interval_t
{
    using real_t = t_real_t;
    using segment_t = t_segment_t;

    using residual_t = residual_t<real_t>;

    using function_sample_t = function_sample_t<real_t>;
    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;

    segment_defects_t segment_defects;
    residual_t residual;
    segment_t segment;

    /// orders by residual.weighted_error, then by left.x
    constexpr auto operator<=>(interval_t const& src) const noexcept -> std::partial_ordering
    {
        auto const lhs_must_subdivide = segment_defects != segment_defects_t{0};
        auto const rhs_must_subdivide = src.segment_defects != segment_defects_t{0};
        if (auto const cmp = lhs_must_subdivide <=> rhs_must_subdivide; std::is_neq(cmp)) return cmp;
        if (auto const cmp = residual.weighted_error <=> src.residual.weighted_error; std::is_neq(cmp)) return cmp;
        return left.x <=> src.left.x;
    }
    constexpr auto operator==(interval_t const& src) const noexcept -> bool = default;
};

/// result of bisecting an interval
template <typename t_interval_t> struct subdivision_t
{
    using interval_t = t_interval_t;

    interval_t left;
    interval_t right;
};

/// estimates residual over a subdomain
template <std::floating_point real_t, typename node_generator_t, typename quantizer_t, typename error_norm_t,
    typename weight_function_t>
struct residual_estimator_t
{
    using residual_t = residual_t<real_t>;

    [[no_unique_address]] node_generator_t generate_nodes;
    [[no_unique_address]] quantizer_t quantize;

    error_norm_t measure_error;
    weight_function_t apply_weight;

    constexpr auto operator()(auto const& sample_target_function, auto const& approximant, real_t left,
        real_t right) const noexcept -> residual_t
    {
        auto max_residual = residual_t{};

        auto const interval_width = (right - left) * 0.5;

        // sample function at generated nodes
        for (auto const standard_node : generate_nodes())
        {
            // convert from standard nodes in [0, 1] to domain nodes in [left, right].
            auto const domain_node = left + standard_node * interval_width;

            // evaluate target function at target position and approxmant at quantized position
            auto const quantized_node = quantize(domain_node);
            auto const approximation = approximant(quantized_node);
            auto const target_function_sample = sample_target_function(domain_node);
            auto const target = target_function_sample.y;
            auto const metric_error = measure_error(target, approximation);
            assert(std::isfinite(metric_error));

            max_residual.scale = max(max_residual.scale, abs(primal(target)));
            max_residual.metric_error = max(max_residual.metric_error, metric_error);
        }

        auto const midpoint = (left + right) * 0.5;
        auto const weight = apply_weight(midpoint);
        assert(std::isfinite(weight));
        max_residual.weighted_error = max_residual.metric_error * weight;
        return max_residual;
    }
};

/// constructs intervals
template <typename t_interval_t, typename approximant_t, typename segment_builder_t, typename defect_analyzer_t,
    typename residual_estimator_t>
struct interval_builder_t
{
    using interval_t = t_interval_t;

    using real_t = interval_t::real_t;
    using function_sample_t = function_sample_t<real_t>;

    [[no_unique_address]] segment_builder_t build_segment;
    [[no_unique_address]] defect_analyzer_t analyze_defects;
    residual_estimator_t estimate_residual;

    constexpr auto build(auto const& sample_target_function, function_sample_t const& left,
        function_sample_t const& midpoint, function_sample_t const& right, int_t log2_width) const noexcept
        -> interval_t
    {
        using x_t = approximant_t::x_t;

        auto const x0 = to_fixed<x_t>(left.x);
        auto const segment = build_segment(left.y, right.y, log2_width);

        return {
            .left = left,
            .midpoint = midpoint,
            .right = right,
            .segment_defects = analyze_defects(segment.coeffs()),
            .residual
            = estimate_residual(sample_target_function, approximant_t{.x0 = x0, .segment = segment}, left.x, right.x),
            .segment = segment,
        };
    }
};

/// bisects intervals, unconditionally
template <typename t_subdivision_t, typename interval_builder_t> struct bisector_t
{
    using subdivision_t = t_subdivision_t;
    using interval_t = subdivision_t::interval_t;

    [[no_unique_address]] interval_builder_t interval_builder;

    constexpr auto operator()(auto const& sample_target_function, interval_t const& parent) const noexcept
        -> subdivision_t
    {
        auto const child_log2_width = parent.segment.log2_width() - 1;

        auto const left_midpoint = sample_target_function(std::midpoint(parent.left.x, parent.midpoint.x));
        auto const right_midpoint = sample_target_function(std::midpoint(parent.midpoint.x, parent.right.x));

        auto const left = interval_builder.build(
            sample_target_function, parent.left, left_midpoint, parent.midpoint, child_log2_width);
        auto const right = interval_builder.build(
            sample_target_function, parent.midpoint, right_midpoint, parent.right, child_log2_width);

        return {
            .left = left,
            .right = right,
        };
    }
};

template <is_fixed t_x_t, typename segment_t> struct completed_segment_t
{
    using x_t = t_x_t;

    x_t origin;
    segment_t segment;

    constexpr auto operator<=>(completed_segment_t const& src) const noexcept -> std::strong_ordering
    {
        return origin <=> src.origin;
    }

    constexpr auto operator==(completed_segment_t const& src) const noexcept -> bool = default;
};

// tracks errors about why subdivision failed and where
template <std::floating_point real_t> struct subdivision_error_t
{
    segment_defects_t defects;

    real_t left;
    real_t right;
};

/// runs subdivision loop over queue and completed segments, returns residual max or reason subdivision failed and where
template <std::floating_point real_t, typename bisector_t, int_t log2_min_width> struct subdivider_t
{
    using subdivision_t = bisector_t::subdivision_t;
    using interval_t = bisector_t::interval_t;

    using subdivision_error_t = subdivision_error_t<real_t>;

    static constexpr auto relative_noise_margin = std::numeric_limits<real_t>::epsilon() * real_t{64};

    [[no_unique_address]] bisector_t bisect;

    struct interval_complete_t
    {};
    using result_t = std::variant<interval_complete_t, subdivision_t, subdivision_error_t>;

    constexpr auto operator()(interval_t const& interval, auto const& sample_target_function,
        real_t global_tolerance) const noexcept -> result_t
    {
        auto const noise_floor = interval.residual.scale * relative_noise_margin;
        auto const local_tolerance = max(global_tolerance, noise_floor);
        auto const can_subdivide
            = interval.segment.log2_width() > log2_min_width && interval.residual.metric_error >= local_tolerance;
        auto const must_subdivide = interval.segment_defects != segment_defects_t{0};

        if (must_subdivide && !can_subdivide)
        {
            return subdivision_error_t{
                .defects = interval.segment_defects, .left = interval.left.x, .right = interval.right.x};
        }

        auto const should_subdivide = interval.residual.metric_error > local_tolerance;
        if ((must_subdivide || should_subdivide) && can_subdivide) return bisect(sample_target_function, interval);
        else return interval_complete_t{};
    }
};

// seeds queue with initial set of segments
//
// The initial set is either one large segment from edge to edge of the domain, or that large segment, split to fit
// critical points aligned ot widths
template <std::floating_point real_t, typename interval_builder_t, int_t log2_domain_max>
struct refinement_pool_seeder_t
{
    [[no_unique_address]] interval_builder_t interval_builder;

    // for now, just push one whole segment
    constexpr auto operator()(auto& queue, auto const& sample_target_function) const -> void
    {
        assert(queue.empty());
        auto const domain_max = std::ldexp(1.0, int_cast<int>(log2_domain_max));
        auto const left = real_t{0};
        auto const right = domain_max;
        queue.push(interval_builder.build(sample_target_function, sample_target_function(left),
            sample_target_function(std::midpoint(left, right)), sample_target_function(right), log2_domain_max));
    }
};

template <std::floating_point real_t, typename refinement_pool_t, typename refinement_pool_seeder_t,
    typename subdivider_t, typename completed_segments_t, int_t max_segment_count>
struct spliner_t
{
    using subdivision_error_t = subdivider_t::subdivision_error_t;
    using interval_t = subdivider_t::interval_t;
    using residual_t = interval_t::residual_t;

    [[no_unique_address]] subdivider_t subdivide;
    [[no_unique_address]] refinement_pool_seeder_t seed_refinement_pool;

    std::vector<interval_t> completed_intervals;
    refinement_pool_t refinement_pool;

    struct result_t
    {
        completed_segments_t completed_segments;
    };

    // optional overload to take a function directly
    template <typename target_function_t>
    auto operator()(target_function_t sample_target_function, real_t global_tolerance)
        -> std::expected<result_t, subdivision_error_t>
    {
        return operator()(function_sampler_t<target_function_t>{std::move(sample_target_function)}, global_tolerance);
    }

    template <typename target_function_t>
    auto operator()(function_sampler_t<target_function_t> sample_target_function, real_t global_tolerance)
        -> std::expected<result_t, subdivision_error_t>
    {
        using subdivision_t = subdivider_t::subdivision_t;
        using completed_segment_t = completed_segments_t::value_type;
        using x_t = completed_segment_t::x_t;

        assert(refinement_pool.empty());
        assert(completed_intervals.empty());

        auto completed_segments = completed_segments_t{};
        completed_segments.reserve(max_segment_count);
        completed_intervals.reserve(max_segment_count);
        refinement_pool.reserve(max_segment_count);

        seed_refinement_pool(refinement_pool, sample_target_function);
        assert(!refinement_pool.empty());

        // subdivide until full
        while (!refinement_pool.empty() && refinement_pool.size() + completed_intervals.size() < max_segment_count)
        {
            // this uses a *reference*; pop must be very specifically placed
            auto const& interval = refinement_pool.top();

            auto const subdivision_result = subdivide(interval, sample_target_function, global_tolerance);
            if (auto const* err = std::get_if<subdivision_error_t>(&subdivision_result)) return std::unexpected(*err);
            else if (auto const* subdivision = std::get_if<subdivision_t>(&subdivision_result))
            {
                refinement_pool.pop();
                refinement_pool.push(subdivision->left);
                refinement_pool.push(subdivision->right);
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

        // convert from segments to intervals
        std::ranges::transform(
            completed_intervals, std::back_inserter(completed_segments), [](auto const& interval) noexcept {
                return completed_segment_t{
                    .origin = to_fixed<x_t>(interval.left.x),
                    .segment = interval.segment,
                };
            });

        // sort by origin
        std::ranges::sort(completed_segments, std::ranges::less{}, &completed_segment_t::origin);

        return result_t{.completed_segments = std::move(completed_segments)};
    }
};

TEST(spline_builder, poc)
{
    using real_t = float_t;

    using x_t = fixed_t<uint64_t, 44>;
    using y_t = fixed_t<uint64_t, 48>;
    using normalized_t = fixed_t<uint64_t, 64>;

    using coeff_t = fixed_t<int64_t, 47>;

    constexpr auto max_segment_count = 1 << 8;
    static constexpr auto log2_domain_max = 8;
    static constexpr auto log2_min_width = -16;
    static auto const min_width = std::ldexp(1.0, log2_min_width);

    using polynomial_evaluator_t = polynomial_evaluator_t<mac_t{}>;
    using packed_segment_t = packed_segment_t<coeff_t>;
    using segment_t = segment_t<x_t, y_t, coeff_t, normalized_t, packed_segment_t, polynomial_evaluator_t{}>;
    using interval_t = interval_t<real_t, segment_t>;
    using refinement_pool_t = priority_queue_t<std::vector<interval_t>>;
    using node_generator_t = node_generators::equioscillation_t<real_t>;
    using quantizer_t = quantizers::fixed_point_t<real_t, x_t::frac_bits>;
    using error_norm_t = error_norms::first_order_relative_t<real_t, error_norms::logsumexp_floor_t<real_t>>;
    using weight_function_t = weight_functions::hyperbolic_decay_t<real_t>;
    using residual_estimator_t
        = residual_estimator_t<real_t, node_generator_t, quantizer_t, error_norm_t, weight_function_t>;
    using segment_derivative_t = segment_derivative_t<real_t>;
    using hermite_converter_t = hermite_converter_t<coeff_t>;
    using segment_builder_t = segment_factory_t<real_t, segment_t, segment_derivative_t, hermite_converter_t>;
    using defect_analyzer_t
        = defect_analyzer_t<defect_checks::monotonicity_t, defect_checks::overflow_t<real_t, normalized_t, mac_t{}>>;
    using approximant_t = approximant_t<real_t, segment_t, segment_derivative_t>;
    using interval_builder_t
        = interval_builder_t<interval_t, approximant_t, segment_builder_t, defect_analyzer_t, residual_estimator_t>;
    using refinement_pool_seeder_t = refinement_pool_seeder_t<real_t, interval_builder_t, log2_domain_max>;
    using subdivision_t = subdivision_t<interval_t>;
    using bisector_t = bisector_t<subdivision_t, interval_builder_t>;
    using completed_segment_t = completed_segment_t<x_t, segment_t>;
    using completed_segments_t = std::vector<completed_segment_t>;
    using subdivider_t = subdivider_t<real_t, bisector_t, log2_min_width>;
    using spliner_t = spliner_t<real_t, refinement_pool_t, refinement_pool_seeder_t, subdivider_t, completed_segments_t,
        max_segment_count>;

    auto const error_norm = error_norm_t{.primal_floor = min_width, .tangent_floor = min_width};
    auto const estimate_residual = residual_estimator_t{
        .generate_nodes = {},
        .quantize = {},
        .measure_error = error_norm,
        .apply_weight = weight_function_t{.halflife = 0.5},
    };

    auto const interval_builder = interval_builder_t{
        .build_segment = {},
        .analyze_defects = {},
        .estimate_residual = estimate_residual,
    };

    auto spliner = spliner_t{
        .subdivide = subdivider_t{
            .bisect = bisector_t{
                .interval_builder = interval_builder
            },
        },
        .seed_refinement_pool={.interval_builder = interval_builder},
        .completed_intervals={},
        .refinement_pool={}
    };

    auto const result = spliner(
        [](auto x) static noexcept -> decltype(x) {
            using std::log1p;
            return log1p(x);
        },
        1e-8);

    EXPECT_TRUE(result.has_value());

    auto const& completed_segments = result.value().completed_segments;

#if 1
    for (auto const completed_segment : completed_segments)
    {
        using std::log1p;

        auto const log2_width = completed_segment.segment.log2_width();
        auto const width = log2_width < 0 ? x_t{1} >> -log2_width : x_t{1} << log2_width;

        static auto const x_denom = 10;
        for (auto x_numer = 0; x_numer < 10; ++x_numer)
        {
            auto const x = width * x_numer / x_denom;
            auto const x_fixed = completed_segment.origin + x;
            auto const x_real = from_fixed<real_t>(x_fixed);
            auto const expected_y = log1p(x_real);
            auto const segment = completed_segment.segment;
            auto const actual_y = from_fixed<real_t>(segment.evaluate(segment.x_to_t(x)));
            auto const difference = actual_y - expected_y;

            std::cout << std::setprecision(4) << "x = " << x_real << ", log1p(x) = " << expected_y
                      << ", y_actual = " << actual_y << ", Δy = " << difference << std::endl;
        }
        std::cout << std::endl;
    }
#endif
}

} // namespace
} // namespace spline
} // namespace crv
