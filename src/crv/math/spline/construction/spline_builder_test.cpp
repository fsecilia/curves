// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline_builder.hpp"
#include <crv/test/test.hpp>

#include <crv/bitwise_enum.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/construction/defect_detectors/monotonicity.hpp>
#include <crv/math/spline/construction/defect_detectors/overflow.hpp>
#include <crv/math/spline/construction/error_norm.hpp>
#include <crv/math/spline/construction/node_generator.hpp>
#include <crv/math/spline/construction/quantizer.hpp>
#include <crv/math/spline/construction/weight_function.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <crv/priority_queue.hpp>
#include <cmath>
#include <expected>
#include <numeric>

#include <iomanip>

namespace crv {
namespace spline {
namespace {

template <typename value_t> using cubic_monomial_t = cubic_polynomial_t<value_t>;
constexpr int_t cubic_coeff_count = 4;

enum class bisection_error_reason_t : int_t
{
    monotonicity = 1 << 0,
    saturation = 1 << 1,
};

} // namespace
} // namespace spline

template <> inline constexpr auto bitwise_for_enum_enabled<spline::bisection_error_reason_t> = true;

namespace spline {
namespace {

/// fixed-point normalized monomial with width
template <typename t_x_t, typename t_y_t, typename t_normalized_t, typename t_coeff_t,
    auto dx_to_t_shifter = shifter_t<rounding_modes::shr::nearest_up>{},
    auto eval_shifter = shifter_t<rounding_modes::shr::truncate>{}>
struct segment_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;
    using normalized_t = t_normalized_t;
    using coeff_t = t_coeff_t;

    using monomial_t = cubic_monomial_t<coeff_t>;
    static auto const coeff_count = cubic_coeff_count;

    monomial_t monomial;
    int_t log2_width;

    constexpr auto evaluate(x_t dx) const noexcept -> y_t
    {
        // calc t in one shift
        auto const dx_to_t_shift = normalized_t::frac_bits - x_t::frac_bits - log2_width;
        auto const t = normalized_t::literal(dx_to_t_shifter.shift(dx.value, dx_to_t_shift));

        auto result = monomial[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff)
        {
            // convert with wrap and no rounding
            result = coeff_t::template convert<overflow_policy_t::wrap, eval_shifter>(multiply(result, t));
            result += monomial[coeff];
        }

        return y_t::convert(result);
    }
};

/// adapts segment with float api, anchors to origin in fixed x
template <std::floating_point real_t, typename segment_t> struct approximant_t
{
    using x_t = segment_t::x_t;

    x_t x_origin;
    segment_t segment;

    constexpr auto operator()(real_t x) const noexcept -> real_t
    {
        // this is *NOT* using a jet, which breaks sobolev.
        // but running a jet through a fixed-point cubic seems like a bad idea
        // we can add a derivative method to the cubic and just compose a real_t jet here instead
        // will sobolev actually benefit?

        auto const dx = to_fixed<x_t>(x) - x_origin;
        return from_fixed<real_t>(segment.evaluate(dx));
    }
};

/// converts float hermite to fixed monomial
template <typename jet_t, typename coeff_t> struct hermite_to_monoimial_converter_t
{
    constexpr auto operator()(jet_t left, jet_t right) const noexcept -> cubic_monomial_t<coeff_t>
    {
        auto const p0 = primal(left);
        auto const p1 = primal(right);
        auto const m0 = tangent(left);
        auto const m1 = tangent(right);
        auto const dp = p1 - p0;
        return {{
            to_fixed<coeff_t>(-2.0 * dp + m0 + m1),
            to_fixed<coeff_t>(3.0 * dp - 2.0 * m0 - m1),
            to_fixed<coeff_t>(m0),
            to_fixed<coeff_t>(p0),
        }};
    }
};

/// builds fixed segment from float hermite and log2_width; scales tangents by width
template <typename t_real_t, typename t_segment_t, typename hermite_to_monoimial_converter_t> struct segment_builder_t
{
    using real_t = t_real_t;
    using segment_t = t_segment_t;

    using jet_t = jet_t<real_t>;

    [[no_unique_address]] hermite_to_monoimial_converter_t hermite_to_monomial;

    constexpr auto operator()(jet_t left, jet_t right, int_t log2_width) const noexcept -> segment_t
    {
        auto const width = std::ldexp(1.0, int_cast<int>(log2_width));
        left.df *= width;
        right.df *= width;
        return segment_t{hermite_to_monomial(left, right), log2_width};
    }
};

/// float x and jet y result of sampling a function at x
template <std::floating_point real_t> struct function_sample_t
{
    real_t x;
    jet_t<real_t> y;
};

/// max scale of target and error between target and approximant over a subdomain
template <std::floating_point real_t> struct residual_t
{
    real_t max_error;
    real_t max_scale;
};

/// unit of work over subdomain
template <std::floating_point t_real_t, typename t_segment_t> struct interval_t
{
    using segment_t = t_segment_t;
    using real_t = t_real_t;
    using residual_t = residual_t<real_t>;

    using function_sample_t = function_sample_t<real_t>;
    function_sample_t left;
    function_sample_t right;

    real_t tolerance;
    int_t log2_width;

    residual_t residual;
    segment_t segment;

    /// orders by residual.max_error, then by left.x
    constexpr auto operator<=>(interval_t const& src) const noexcept -> std::partial_ordering
    {
        if (auto const cmp = residual.max_error <=> src.residual.max_error; std::is_neq(cmp)) return cmp;
        return left.x <=> src.left.x;
    }
    constexpr auto operator==(interval_t const& src) const noexcept -> bool = default;
};

/// result of bisecting an interval
template <typename t_interval_t> struct bisection_t
{
    using interval_t = t_interval_t;
    using real_t = interval_t::real_t;
    using residual_t = residual_t<real_t>;

    interval_t left;
    interval_t right;
    residual_t refined_residual;
};

/// estimates residual over a subdomain
template <std::floating_point real_t, typename node_generator_t, typename quantizer_t, typename error_norm_t,
    typename weight_function_t>
struct residual_estimator_t
{
    using residual_t = residual_t<real_t>;

    [[no_unique_address]] error_norm_t measure_error;
    [[no_unique_address]] weight_function_t apply_weight;
    [[no_unique_address]] node_generator_t generate_nodes;
    [[no_unique_address]] quantizer_t quantize;

    auto operator()(auto const& target_function, auto const& approximant, real_t left, real_t right,
        real_t midpoint) const noexcept -> residual_t
    {
        auto const error_weight = apply_weight(midpoint);
        auto const half_width = (right - left) * 0.5;

        auto max_scale = real_t{0};
        auto max_error = real_t{0};

        // sample function at generated nodes
        for (auto const standard_node : generate_nodes())
        {
            auto const domain_node = midpoint + half_width * standard_node;

            // evaluate target function at target position and approxmant at quantized position
            auto const quantized_node = quantize(domain_node);
            auto const target = target_function(domain_node);
            auto const approximation = approximant(quantized_node);
            max_scale = std::max(max_scale, abs(target));

            // measure magnitude of error using norm, weight it using weight function
            auto const error = measure_error(jet_t{target}, jet_t{approximation});
            auto const weighted_error = error * error_weight;
            max_error = std::max(max_error, weighted_error);
        }

        return residual_t{.max_error = max_error, .max_scale = max_scale};
    }
};

/// constructs intervals
template <typename t_interval_t, typename residual_estimator_t, typename segment_builder_t> struct interval_builder_t
{
    using interval_t = t_interval_t;
    using segment_t = interval_t::segment_t;
    using real_t = interval_t::real_t;
    using jet_t = jet_t<real_t>;
    using function_sample_t = function_sample_t<real_t>;

    using approximant_t = approximant_t<real_t, segment_t>;
    using x_t = approximant_t::x_t;

    [[no_unique_address]] residual_estimator_t estimate_residual;
    [[no_unique_address]] segment_builder_t build_segment;

    constexpr auto sample_function(auto const& target_function, real_t x) const noexcept -> function_sample_t
    {
        return {.x = x, .y = target_function(jet_t{x, 1.0})};
    }

    constexpr auto build(auto const& target_function, function_sample_t const& left, function_sample_t const& right,
        function_sample_t const& midpoint, real_t tolerance, int_t log2_width) const noexcept -> interval_t
    {
        auto const x_origin = to_fixed<x_t>(left.x);
        auto const segment = build_segment(left.y, right.y, log2_width);

        return {
            .left = left,
            .right = right,
            .tolerance = tolerance,
            .log2_width = log2_width,
            .residual = estimate_residual(
                target_function, approximant_t{.x_origin = x_origin, .segment = segment}, left.x, right.x, midpoint.x),
            .segment = segment,
        };
    }
};

/// bisects intervals, unconditionally
template <typename t_bisection_t, typename interval_builder_t> struct bisector_t
{
    using bisection_t = t_bisection_t;
    using interval_t = bisection_t::interval_t;
    using segment_t = interval_t::segment_t;
    using real_t = interval_t::real_t;
    using jet_t = jet_t<real_t>;
    using function_sample_t = function_sample_t<real_t>;

    [[no_unique_address]] interval_builder_t interval_builder;

    constexpr auto operator()(auto const& target_function, interval_t const& parent) const noexcept -> bisection_t
    {
        auto const midpoint
            = interval_builder.sample_function(target_function, std::midpoint(parent.left.x, parent.right.x));

        // errors in a spline are max, not distributed like with quadrature; do not track this: always use global
        auto const child_tolerance = parent.tolerance; // / 2;

        auto const child_log2_width = parent.log2_width - 1;

        auto const left_midpoint
            = interval_builder.sample_function(target_function, std::midpoint(parent.left.x, midpoint.x));
        auto const right_midpoint
            = interval_builder.sample_function(target_function, std::midpoint(midpoint.x, parent.right.x));

        auto const left = interval_builder.build(
            target_function, parent.left, midpoint, left_midpoint, child_tolerance, child_log2_width);
        auto const right = interval_builder.build(
            target_function, midpoint, parent.right, right_midpoint, child_tolerance, child_log2_width);

        auto const refined_residual = residual_t{
            .max_error = std::max(left.residual.max_error, right.residual.max_error),
            .max_scale = std::max(left.residual.max_scale, right.residual.max_scale),
        };

        return {
            .left = left,
            .right = right,
            .refined_residual = refined_residual,
        };
    }
};

template <is_fixed t_x_t, typename segment_t> struct completed_segment_t
{
    using x_t = t_x_t;
    x_t origin;
    int_t log2_width;
    segment_t segment;

    constexpr auto operator<=>(completed_segment_t const& src) const noexcept -> auto { return origin <=> src.origin; }
    constexpr auto operator==(completed_segment_t const& src) const noexcept -> bool = default;
};

// tracks errors about why bisection failed and where
template <std::floating_point real_t> struct bisection_error_t
{
    bisection_error_reason_t reasons;

    real_t left;
    real_t right;
};

// this is named poorly
template <typename monotonicity_t, typename overflow_t> struct refinement_policies_t
{
    [[no_unique_address]] monotonicity_t monotonicity;
    [[no_unique_address]] overflow_t overflow;

    template <typename coeff_t>
    auto operator()(cubic_monomial_t<coeff_t> const& monomial) const noexcept -> bisection_error_reason_t
    {
        auto result = bisection_error_reason_t{};
        if (monotonicity(monomial)) result |= bisection_error_reason_t::monotonicity;
        if (overflow(monomial)) result |= bisection_error_reason_t::saturation;
        return result;
    }
};

/// runs subdivision loop over queue and completed segments, returns residual max or reason subdivision failed and where
template <std::floating_point real_t, typename bisector_t, typename refinement_policies_t, int_t max_segment_count>
struct subdivider_t
{
    using bisection_t = bisector_t::bisection_t;
    using interval_t = bisector_t::interval_t;
    using residual_t = residual_t<real_t>;

    using bisection_error_t = bisection_error_t<real_t>;

    static constexpr auto min_log2_width = -16;
    static constexpr auto relative_noise_margin = std::numeric_limits<real_t>::epsilon() * real_t{64};

    [[no_unique_address]] bisector_t bisect;
    [[no_unique_address]] refinement_policies_t refinement_policies;

    residual_t residual_max{};

    constexpr auto subdivide(auto& queue, auto const& target_function) noexcept
        -> std::expected<residual_t, bisection_error_t>
    {
        assert(!queue.empty());

        while (queue.size() < max_segment_count && !queue.empty())
        {
            auto const interval = queue.top();

            auto const refinement_reasons = refinement_policies(interval.segment.monomial);
            auto const must_subdivide = refinement_reasons != bisection_error_reason_t{0};

            auto const noise_floor = interval.residual.max_scale * relative_noise_margin;
            auto const local_tolerance = std::max(interval.tolerance, noise_floor);
            auto const can_subdivide
                = interval.log2_width > min_log2_width && interval.residual.max_error >= local_tolerance;

            if (must_subdivide && !can_subdivide)
            {
                return std::unexpected(bisection_error_t{
                    .reasons = refinement_reasons, .left = interval.left.x, .right = interval.right.x});
            }

            auto const should_subdivide = interval.residual.max_error > local_tolerance;
            if (!should_subdivide) return interval.residual;

            auto const bisection = bisect(target_function, interval);
            residual_max.max_error = std::max(residual_max.max_error, bisection.refined_residual.max_error);
            residual_max.max_scale = std::max(residual_max.max_scale, bisection.refined_residual.max_scale);

            queue.pop();
            queue.push(bisection.left);
            queue.push(bisection.right);
        }

        return queue.empty() ? residual_max : queue.top().residual;
    }
};

// seeds queue with initial set of segments
//
// The initial set is either one large segment from edge to edge of the domain, or that large segment, split to fit
// critical points aligned ot widths
template <std::floating_point real_t, typename interval_builder_t> struct queue_seeder_t
{
    [[no_unique_address]] interval_builder_t interval_builder;

    // for now, just push one whole segment
    constexpr auto operator()(
        auto& queue, auto const& target_function, int_t log2_domain_max, real_t global_tolerance) const -> void
    {
        assert(queue.empty());
        auto const domain_max = std::ldexp(1.0, int_cast<int>(log2_domain_max));
        auto const left = real_t{0};
        auto const right = domain_max;
        auto const tolerance = global_tolerance * ((right - left) / domain_max);
        queue.push(interval_builder.build(target_function, interval_builder.sample_function(target_function, left),
            interval_builder.sample_function(target_function, right),
            interval_builder.sample_function(target_function, std::midpoint(left, right)), tolerance, log2_domain_max));
    }
};

template <std::floating_point real_t, typename queue_t, typename queue_seeder_t, typename subdivider_t,
    typename completed_segments_t>
struct spliner_t
{
    using bisection_error_t = subdivider_t::bisection_error_t;
    using completed_segment_t = completed_segments_t::value_type;
    using x_t = completed_segment_t::x_t;

    [[no_unique_address]] subdivider_t subdivider;
    [[no_unique_address]] queue_seeder_t seed_queue;
    completed_segments_t completed_segments;

    queue_t queue;

    auto operator()(auto const& target_function, int_t log2_domain_max, real_t global_tolerance)
        -> std::optional<bisection_error_t>
    {
        assert(completed_segments.empty());

        seed_queue(queue, target_function, log2_domain_max, global_tolerance);
        auto const result = subdivider.subdivide(queue, target_function);
        if (!result.has_value()) return result.error();

        completed_segments.reserve(queue.size());
        // completed_segments.assign(std::begin(queue), std::end(queue));
        while (!queue.empty())
        {
            auto const& interval = std::move(queue).top();
            completed_segments.push_back(completed_segment_t{
                .origin = to_fixed<x_t>(interval.left.x),
                .log2_width = interval.log2_width,
                .segment = interval.segment,
            });
            queue.pop();
        }

        std::ranges::sort(
            completed_segments, std::ranges::less{}, [](auto const& elem) static noexcept { return elem.origin; });

        auto const mid_segment_index = completed_segments.size() / 2;
        auto const mid_segment = completed_segments[mid_segment_index];
        std::cout << std::setprecision(10) << "mid segment index: " << mid_segment_index
                  << " .x_origin = " << mid_segment.origin << ", .d = " << mid_segment.segment.monomial[3] << std::endl;

        return std::nullopt;
    }
};

TEST(spline_builder, poc)
{
    using real_t = float_t;
    using jet_t = jet_t<real_t>;

    using x_t = fixed_t<uint64_t, 44>;
    using y_t = fixed_t<uint64_t, 48>;
    using normalized_t = fixed_t<uint64_t, 63>;

    using coeff_t = fixed_t<int64_t, 47>;

    constexpr auto max_segment_count = 1 << 8;

    using segment_t = segment_t<x_t, y_t, normalized_t, coeff_t>;
    using interval_t = interval_t<real_t, segment_t>;
    using queue_t = priority_queue_t<std::vector<interval_t>>;
    using node_generator_t = node_generators::equioscillation_t<real_t>;
    using quantizer_t = quantizers::fixed_point_t<real_t, x_t::frac_bits>;
    using error_norm_t = error_norms::first_order_relative_t<real_t, error_norms::logsumexp_floor_t<real_t>>;
    using weight_function_t = weight_functions::hyperbolic_decay_t<real_t>;
    using residual_estimator_t
        = residual_estimator_t<real_t, node_generator_t, quantizer_t, error_norm_t, weight_function_t>;
    using hermite_to_monoimial_converter_t = hermite_to_monoimial_converter_t<jet_t, coeff_t>;
    using segment_builder_t = segment_builder_t<real_t, segment_t, hermite_to_monoimial_converter_t>;
    using interval_builder_t = interval_builder_t<interval_t, residual_estimator_t, segment_builder_t>;
    using queue_seeder_t = queue_seeder_t<real_t, interval_builder_t>;
    using refinement_policies_t = refinement_policies_t<defect_detectors::monotonicity_t,
        defect_detectors::overflow_t<real_t, normalized_t, mac_t{}>>;
    using bisection_t = bisection_t<interval_t>;
    using bisector_t = bisector_t<bisection_t, interval_builder_t>;
    using subdivider_t = subdivider_t<real_t, bisector_t, refinement_policies_t, max_segment_count>;
    using completed_segment_t = completed_segment_t<x_t, segment_t>;
    using completed_segments_t = std::vector<completed_segment_t>;
    using spliner_t = spliner_t<real_t, queue_t, queue_seeder_t, subdivider_t, completed_segments_t>;

    auto spliner = spliner_t{
        .subdivider = subdivider_t{.bisect = bisector_t{.interval_builder
                                       = interval_builder_t{.estimate_residual = residual_estimator_t{
                                                                .measure_error = error_norm_t{},
                                                                .apply_weight = weight_function_t{.halflife = 0.5},
                                                                .generate_nodes = {},
                                                                .quantize = {},
                                                                },
                                                                .build_segment={},
                                                            },
                                        },
                                        .refinement_policies={}
                                    },
                                    .seed_queue={},
                                    .completed_segments={},
                                    .queue={}
        };

    auto const result = spliner(
        [](auto x) static noexcept -> decltype(x) {
            using std::log1p;
            return log1p(x);
        },
        8, 1e-8);

    EXPECT_FALSE(result.has_value());
}

} // namespace
} // namespace spline
} // namespace crv
