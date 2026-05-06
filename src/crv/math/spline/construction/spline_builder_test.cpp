// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline_builder.hpp"
#include <crv/test/test.hpp>

#include <crv/bitwise_enum.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/elementwise_max.hpp>
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

template <typename value_t> using cubic_monomial_t = cubic_polynomial_t<value_t>;
constexpr int_t cubic_coeff_count = 4;

enum class segment_defects_t : int_t
{
    monotonicity = 1 << 0,
    overflow = 1 << 1,
};

} // namespace
} // namespace spline

template <> inline constexpr auto bitwise_for_enum_enabled<spline::segment_defects_t> = true;

namespace spline {
namespace {

/// single, fixed-point multiply and carry step with normalized t
///
/// This is a standard horner's loop step for fixed-point. It has no mitigation for overflow; it presumes the
/// polynomial's intermediate steps were already checked at the extrema.
///
/// \pre sum known to not overflow
struct fast_mac_step_t
{
    template <is_fixed accumulator_t, is_fixed normalized_t>
    constexpr auto operator()(accumulator_t accumulator, normalized_t t, accumulator_t coeff) const noexcept
        -> accumulator_t
    {
        using narrow_t = typename accumulator_t::value_t;
        using wide_t = widened_t<narrow_t>;
        static constexpr auto rounding_bias = wide_t{1} << (normalized_t::frac_bits - 1);
        auto const wide_product = static_cast<wide_t>(accumulator.value) * t.value + rounding_bias;
        auto const narrow_product = static_cast<narrow_t>(wide_product >> normalized_t::frac_bits);
        return accumulator_t::literal(narrow_product + coeff.value);
    }
};

/// fixed-length horner's loop using fold
template <auto mac_step> struct polynomial_evaluator_t
{
    template <typename coeff_t>
    constexpr auto operator()(
        auto t, coeff_t highest_coeff, std::same_as<coeff_t> auto... remaining_coeffs) const noexcept -> coeff_t
    {
        auto accumulator = highest_coeff;
        ((accumulator = mac_step(accumulator, t, remaining_coeffs)), ...);
        return accumulator;
    }
};

/// normalizes from spline input space to a segment's local input space
template <typename t_normalized_t, auto shifter = shifter_t<rounding_modes::shr::nearest_up>{}>
struct segment_input_normalizer_t
{
    using normalized_t = t_normalized_t;

    template <is_fixed x_t> constexpr auto operator()(x_t x, int_t log2_width) const noexcept -> normalized_t
    {
        auto const x_to_t_shift = normalized_t::frac_bits - x_t::frac_bits - log2_width;
        return normalized_t::literal(shifter.shift(x.value, x_to_t_shift));
    }
};

/// fixed-point normalized polynomial with width
template <typename t_x_t, typename t_y_t, typename t_coeff_t, auto input_normalizer, auto polynomial_evaluator>
struct segment_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;
    using coeff_t = t_coeff_t;
    using normalized_t = decltype(input_normalizer)::normalized_t;

    using polynomial_t = cubic_polynomial_t<coeff_t>;

    polynomial_t polynomial;
    int_t log2_width;

    constexpr auto x_to_t(x_t x) const noexcept -> normalized_t { return input_normalizer(x, log2_width); }

    constexpr auto primal(normalized_t t) const noexcept -> y_t
    {
        return y_t::convert(polynomial_evaluator(t, polynomial[0], polynomial[1], polynomial[2], polynomial[3]));
    }
};

/// converts between dy/dx and dy/dt using the jacobian, dx/dt
struct tangent_jacobian_t
{
    template <std::floating_point real_t> constexpr auto dx_dt(int_t log2_dx_dt) const noexcept -> real_t
    {
        return std::ldexp(real_t{1.0}, int_cast<int>(log2_dx_dt));
    }

    template <std::floating_point real_t>
    constexpr auto dy_dt_to_dy_dx(real_t dy_dt, int_t log2_dx_dt) const noexcept -> real_t
    {
        return std::ldexp(dy_dt, int_cast<int>(-log2_dx_dt));
    }

    template <std::floating_point real_t>
    constexpr auto dy_dx_to_dy_dt(real_t dy_dx, int_t log2_dx_dt) const noexcept -> real_t
    {
        return std::ldexp(dy_dx, int_cast<int>(log2_dx_dt));
    }
};

/// builds fixed segment from float hermite and log2_width; scales tangents by width
template <typename t_real_t, typename t_segment_t, typename tangent_jacobian_t, auto hermite_to_polynomial_converter>
struct segment_builder_t
{
    using real_t = t_real_t;
    using segment_t = t_segment_t;

    using jet_t = jet_t<real_t>;

    [[no_unique_address]] tangent_jacobian_t tangent_jacobian;

    constexpr auto operator()(jet_t left, jet_t right, int_t log2_width) const noexcept -> segment_t
    {
        // convert from spline-global dy/dx to segment-local dy/dt via chain rule
        auto dx_dt = tangent_jacobian.template dx_dt<real_t>(log2_width);
        left.df *= dx_dt;
        right.df *= dx_dt;

        return segment_t{hermite_to_polynomial_converter(left, right), log2_width};
    }
};

/// adapts segment with float api, anchors to origin in fixed x
template <std::floating_point t_real_t, typename segment_t, typename tangent_jacobian_t, auto polynomial_evaluator>
struct approximant_t
{
    using real_t = t_real_t;
    using x_t = segment_t::x_t;

    x_t x_origin;
    segment_t segment;

    [[no_unique_address]] tangent_jacobian_t tangent_jacobian = {};

    constexpr auto operator()(real_t x) const noexcept -> jet_t<real_t>
    {
        auto const t = segment.x_to_t(to_fixed<x_t>(x) - x_origin);
        auto const primal = from_fixed<real_t>(segment.primal(t));

        // convert from segment-local dy/dt to spline-global dy/dx via chain rule
        auto const dy_dt = from_fixed<real_t>(tangent(t));
        auto const dy_dx = tangent_jacobian.dy_dt_to_dy_dx(dy_dt, segment.log2_width);

        return jet_t{primal, dy_dx};
    }

private:
    constexpr auto tangent(segment_t::normalized_t t) const noexcept -> segment_t::y_t
    {
        // TODO: We check that the primal calc does not overflow any intermediate calcs, but not the tangent calc.
        // This may overflow.
        auto const& polynomial = segment.polynomial;
        return segment_t::y_t::convert(polynomial_evaluator(t, 3 * polynomial[0], 2 * polynomial[1], polynomial[2]));
    }
};

/// uniform error norm
template <typename scalar_t, typename jet_t> struct uniform_t
{
    static constexpr auto operator()(jet_t target, jet_t approximation) noexcept -> scalar_t
    {
        auto const result = abs(primal(target) - primal(approximation));
        assert(std::isfinite(result));
        return result;
    }
};

// soft floor: behaves like floor when value << floor, like value when value >> floor
template <typename real_t> struct soft_floor_t
{
    static constexpr auto operator()(real_t value, real_t floor) noexcept -> real_t
    {
        using std::hypot;
        using std::isfinite;

        auto const result = hypot(value, floor);
        assert(isfinite(result));
        return result;
    }
};

/// first-order uniform norm with target-derived relative-slope weighting
template <typename jet_t, int_t log2_min_width, typename soft_floor_t> struct first_order_relative_t
{
    using scalar_t = typename jet_t::value_t;

    // floor prevents weight blowup where target slope is near zero or unresolvable
    static constexpr auto primal_floor = scalar_t{scalar_t{1} / (1 << -log2_min_width)};
    static constexpr auto tangent_floor = scalar_t{scalar_t{1} / (1 << -log2_min_width)};

    [[no_unique_address]] soft_floor_t soft_floor;

    constexpr auto operator()(jet_t target, jet_t approximation) const noexcept -> scalar_t
    {
        // weight derived from the target itself: 1 / max(|target_slope|, floor)
        // this turns absolute slope error into relative slope error in flat regions

        auto const primal_error = abs(primal(target) - primal(approximation));
        auto const tangent_error = abs(tangent(target) - tangent(approximation));

        auto const primal_scale = soft_floor(abs(primal(target)), primal_floor);
        auto const relative_primal_error = primal_error / primal_scale;
        assert(std::isfinite(relative_primal_error));

        auto const tangent_scale = soft_floor(abs(tangent(target)), tangent_floor);
        auto const relative_tangent_error = tangent_error / tangent_scale;
        assert(std::isfinite(relative_tangent_error));

        return max(relative_primal_error, relative_tangent_error);
    }
};

/// converts float hermite to fixed polynomial
template <typename jet_t, typename coeff_t> struct hermite_to_polynomial_converter_t
{
    constexpr auto operator()(jet_t left, jet_t right) const noexcept -> cubic_polynomial_t<coeff_t>
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

/// float x and jet y result of sampling a function at x
template <std::floating_point real_t> struct function_sample_t
{
    real_t x;
    jet_t<real_t> y;
};

// samples target function, returning the sample location and resulting 1-jet
template <typename target_function_t> struct function_sampler_t
{
    target_function_t target_function;

    template <std::floating_point real_t>
    constexpr auto operator()(real_t x) const noexcept -> function_sample_t<real_t>
    {
        return {.x = x, .y = target_function(jet_t<real_t>{x, 1.0})};
    }
};

/// max scale of target and error between target and approximant over a subdomain
template <std::floating_point real_t> struct residual_t
{
    real_t primal_error; // L-infinity of primal approx relative to target
    real_t tangent_error; // L-infinity of tangent approx relative to target
    real_t metric_error; // error based on norm error metric
    real_t weighted_error; // metric_error weighted perceptually
    real_t scale; // absolute magnitude of primal

    friend auto elementwise_max(residual_t const& lhs, residual_t const& rhs) noexcept -> residual_t
    {
        using cpo::elementwise_max;
        return residual_t{
            .primal_error = elementwise_max(lhs.primal_error, rhs.primal_error),
            .tangent_error = elementwise_max(lhs.tangent_error, rhs.tangent_error),
            .metric_error = elementwise_max(lhs.metric_error, rhs.metric_error),
            .weighted_error = elementwise_max(lhs.weighted_error, rhs.weighted_error),
            .scale = elementwise_max(lhs.scale, rhs.scale),
        };
    }

    friend auto operator<<(std::ostream& out, residual_t const& src) -> std::ostream&
    {
        return out << "{.primal_error = " << src.primal_error << ", .tangent_error = " << src.tangent_error
                   << ", .metric_error = " << src.metric_error << ", .weighted_error = " << src.weighted_error
                   << ", .scale = " << src.scale << "}";
    }
};

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

    constexpr auto operator()(auto const& sample_target_function, auto const& approximant, real_t interval_width,
        real_t midpoint) const noexcept -> residual_t
    {
        auto max_residual = residual_t{};

        // sample function at generated nodes
        auto const half_width = interval_width * 0.5;
        for (auto const standard_node : generate_nodes())
        {
            auto const domain_node = midpoint + half_width * standard_node;

            // evaluate target function at target position and approxmant at quantized position
            auto const quantized_node = quantize(domain_node);
            auto const approximation = approximant(quantized_node);
            auto const target_function_sample = sample_target_function(domain_node);
            auto const target = target_function_sample.y;
            auto const primal_error = abs(primal(target) - primal(approximation));
            auto const tangent_error = abs(tangent(target) - tangent(approximation));
            auto const metric_error = measure_error(target, approximation);
            assert(std::isfinite(metric_error));

            max_residual.scale = max(max_residual.scale, abs(primal(target)));
            max_residual.primal_error = max(max_residual.primal_error, primal_error);
            max_residual.tangent_error = max(max_residual.tangent_error, tangent_error);
            max_residual.metric_error = max(max_residual.metric_error, metric_error);
        }

        auto const weight = apply_weight(midpoint);
        assert(std::isfinite(weight));
        max_residual.weighted_error = max_residual.metric_error * weight;
        return max_residual;
    }
};

/// constructs intervals
template <typename t_interval_t, typename approximant_t, typename segment_builder_t, typename defect_detector_t,
    typename residual_estimator_t>
struct interval_builder_t
{
    using interval_t = t_interval_t;

    using real_t = interval_t::real_t;
    using function_sample_t = function_sample_t<real_t>;

    [[no_unique_address]] segment_builder_t build_segment;
    [[no_unique_address]] defect_detector_t detect_defects;
    residual_estimator_t estimate_residual;

    constexpr auto build(auto const& sample_target_function, function_sample_t const& left,
        function_sample_t const& midpoint, function_sample_t const& right, int_t log2_width) const noexcept
        -> interval_t
    {
        using x_t = approximant_t::x_t;

        auto const x_origin = to_fixed<x_t>(left.x);
        auto const segment = build_segment(left.y, right.y, log2_width);

        return {
            .left = left,
            .midpoint = midpoint,
            .right = right,
            .segment_defects = detect_defects(segment.polynomial),
            .residual = estimate_residual(sample_target_function,
                approximant_t{.x_origin = x_origin, .segment = segment}, right.x - left.x, midpoint.x),
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
        auto const child_log2_width = parent.segment.log2_width - 1;

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

// this is named poorly
template <typename monotonicity_t, typename overflow_t> struct defect_detector_t
{
    [[no_unique_address]] monotonicity_t monotonicity;
    [[no_unique_address]] overflow_t overflow;

    template <typename coeff_t>
    auto operator()(cubic_polynomial_t<coeff_t> const& polynomial) const noexcept -> segment_defects_t
    {
        auto result = segment_defects_t{};
        if (monotonicity(polynomial)) result |= segment_defects_t::monotonicity;
        if (overflow(polynomial)) result |= segment_defects_t::overflow;
        return result;
    }
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
            = interval.segment.log2_width > log2_min_width && interval.residual.metric_error >= local_tolerance;
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
        residual_t max_residual;
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

        // apply max to residual
        auto const max_residual = std::ranges::fold_left(
            completed_intervals, residual_t{}, [](auto const& accumulator, auto const& element) noexcept {
                return cpo::elementwise_max(accumulator, element.residual);
            });

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

        return result_t{.completed_segments = std::move(completed_segments), .max_residual = max_residual};
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
    static constexpr auto log2_min_width = -16;
    static constexpr auto log2_domain_max = 8;

    using segment_input_normalizer_t = segment_input_normalizer_t<normalized_t>;
    using polynomial_evaluator_t = polynomial_evaluator_t<fast_mac_step_t{}>;
    using segment_t = segment_t<x_t, y_t, coeff_t, segment_input_normalizer_t{}, polynomial_evaluator_t{}>;
    using interval_t = interval_t<real_t, segment_t>;
    using refinement_pool_t = priority_queue_t<std::vector<interval_t>>;
    using node_generator_t = node_generators::equioscillation_t<real_t>;
    using quantizer_t = quantizers::fixed_point_t<real_t, x_t::frac_bits>;
    using error_norm_t = error_norms::first_order_relative_t<real_t, error_norms::logsumexp_floor_t<real_t>>;
    using weight_function_t = weight_functions::hyperbolic_decay_t<real_t>;
    using residual_estimator_t
        = residual_estimator_t<real_t, node_generator_t, quantizer_t, error_norm_t, weight_function_t>;
    using hermite_to_polynomial_converter_t = hermite_to_polynomial_converter_t<jet_t, coeff_t>;
    using segment_builder_t
        = segment_builder_t<real_t, segment_t, tangent_jacobian_t, hermite_to_polynomial_converter_t{}>;
    using defect_detector_t = defect_detector_t<defect_detectors::monotonicity_t,
        defect_detectors::overflow_t<real_t, normalized_t, mac_t{}>>;
    using approximant_t = approximant_t<real_t, segment_t, tangent_jacobian_t, polynomial_evaluator_t{}>;
    using interval_builder_t
        = interval_builder_t<interval_t, approximant_t, segment_builder_t, defect_detector_t, residual_estimator_t>;
    using refinement_pool_seeder_t = refinement_pool_seeder_t<real_t, interval_builder_t, log2_domain_max>;
    using subdivision_t = subdivision_t<interval_t>;
    using bisector_t = bisector_t<subdivision_t, interval_builder_t>;
    using completed_segment_t = completed_segment_t<x_t, segment_t>;
    using completed_segments_t = std::vector<completed_segment_t>;
    using subdivider_t = subdivider_t<real_t, bisector_t, log2_min_width>;
    using spliner_t = spliner_t<real_t, refinement_pool_t, refinement_pool_seeder_t, subdivider_t, completed_segments_t,
        max_segment_count>;

    auto const estimate_residual = residual_estimator_t{
        .generate_nodes = {},
        .quantize = {},
        .measure_error = error_norm_t{},
        .apply_weight = weight_function_t{.halflife = 0.5},
    };

    auto const interval_builder = interval_builder_t{
        .build_segment = {},
        .detect_defects = {},
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

        auto const log2_width = completed_segment.segment.log2_width;
        auto const width = log2_width < 0 ? x_t{1} >> -log2_width : x_t{1} << log2_width;

        static auto const dx_denom = 10;
        for (auto dx_numer = 0; dx_numer <= 10; ++dx_numer)
        {
            auto const dx = width * dx_numer / dx_denom;
            auto const x_fixed = completed_segment.origin + dx;
            auto const x_real = from_fixed<real_t>(x_fixed);
            auto const expected_y = log1p(x_real);
            auto const segment = completed_segment.segment;
            auto const actual_y = from_fixed<real_t>(segment.primal(segment.x_to_t(dx)));
            auto const difference = actual_y - expected_y;

            std::cout << std::setprecision(4) << "x = " << x_real << ", log1p(x) = " << expected_y
                      << ", y_actual = " << actual_y << ", Δy = " << difference << std::endl;
        }
        std::cout << std::endl;
    }
    std::cout << "max residual: " << result.value().max_residual << std::endl;
#endif
}

} // namespace
} // namespace spline
} // namespace crv
