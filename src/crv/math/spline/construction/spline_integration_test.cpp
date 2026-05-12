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
#include <crv/math/spline/construction/error_norm.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/hermite_converter.hpp>
#include <crv/math/spline/construction/interval.hpp>
#include <crv/math/spline/construction/node_generator.hpp>
#include <crv/math/spline/construction/residual_estimator.hpp>
#include <crv/math/spline/construction/segment_derivative.hpp>
#include <crv/math/spline/construction/segment_factory.hpp>
#include <crv/math/spline/construction/weight_function.hpp>
#include <crv/math/spline/construction/workspace.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <crv/math/spline/segment.hpp>
#include <crv/math/spline/segment_locator.hpp>
#include <crv/math/spline/spline.hpp>
#include <crv/priority_queue.hpp>
#include <algorithm>
#include <cmath>
#include <expected>
#include <iomanip>
#include <numeric>
#include <stdfloat>

namespace crv {
namespace spline {
namespace {

/// result of bisecting a subdomain
template <typename t_subdomain_t> struct bisection_t
{
    using subdomain_t = t_subdomain_t;

    subdomain_t left;
    subdomain_t right;
};

/// bisects subdomains
template <typename t_bisection_t> struct bisector_t
{
    using bisection_t = t_bisection_t;
    using subdomain_t = bisection_t::subdomain_t;

    constexpr auto operator()(auto const& sample_target_function, subdomain_t const& parent) const noexcept
        -> bisection_t
    {
        auto const child_log2_width = parent.log2_width - 1;
        return
        {
            .left = subdomain_t{
                .left = parent.left,
                .midpoint = sample_target_function(std::midpoint(parent.left.x, parent.midpoint.x)),
                .right = parent.midpoint,
                .log2_width = child_log2_width,
            },
            .right = subdomain_t{
                .left = parent.midpoint,
                .midpoint = sample_target_function(std::midpoint(parent.midpoint.x, parent.right.x)),
                .right = parent.right,
                .log2_width = child_log2_width,
            },
        };
    }
};

/// decides if an interval should subdivide; signals error if interval must subdivide but cannot
template <std::floating_point real_t, int_t log2_min_width> struct convergence_test_t
{
    static constexpr auto relative_noise_margin = std::numeric_limits<real_t>::epsilon() * real_t{64};
    real_t global_tolerance;

    constexpr auto operator()(auto const& interval) const noexcept -> bool
    {
        auto const noise_floor = interval.residual.scale * relative_noise_margin;
        auto const local_tolerance = max(global_tolerance, noise_floor);
        auto const can_subdivide
            = interval.subdomain.log2_width > log2_min_width && interval.residual.metric_error >= local_tolerance;
        auto const should_subdivide = interval.residual.metric_error > local_tolerance;
        return should_subdivide && can_subdivide;
    }
};

/// result of subdividing an interval
template <typename t_interval_t> struct subdivision_t
{
    using interval_t = t_interval_t;

    interval_t left;
    interval_t right;
};

/// subdivides an interval
template <std::floating_point real_t, typename bisector_t, typename interval_factory_t> struct subdivider_t
{
    using interval_t = interval_factory_t::interval_t;
    using subdivision_t = subdivision_t<interval_t>;

    [[no_unique_address]] bisector_t bisect;
    interval_factory_t interval_factory;

    constexpr auto operator()(interval_t const& interval, auto const& sample_target_function) const noexcept
        -> subdivision_t
    {
        auto const child_domains = bisect(sample_target_function, interval.subdomain);
        return subdivision_t{
            .left = interval_factory.create(sample_target_function, child_domains.left),
            .right = interval_factory.create(sample_target_function, child_domains.right),
        };
    }
};

template <typename t_workspace_t> struct typestates_t
{
    using workspace_t = t_workspace_t;

    struct refined_t
    {
        workspace_t& workspace;
    };

    struct seeded_t
    {
        workspace_t& workspace;
        using next_t = refined_t;
    };

    struct unseeded_t
    {
        workspace_t& workspace;
        using next_t = seeded_t;
    };
};

// seeds queue with initial set of segments
//
// The initial set is either one large segment from edge to edge of the domain, or that large segment, split to fit
// critical points aligned ot widths
template <std::floating_point real_t, typename typestate_t, typename interval_factory_t, int_t log2_domain_max,
    int_t domain_max>
struct refinement_pool_seeder_t
{
    interval_factory_t interval_factory;

    constexpr auto operator()(typestate_t state, auto const& sample_target_function) const ->
        typename typestate_t::next_t
    {
        using subdomain_t = subdomain_t<real_t>;

        auto& workspace = state.workspace;
        assert(workspace.empty());

        // unconditionally decimate the curve into 16 segments
        //
        // Our bit packing format is riding the edge of overflow during the initial seed if it has a width of 256.
        // We can either use more integer bits, or decimate the curve. Here, we split the initial range into 16 segments
        // unconditionally. This is temporary until we support arbitary critical points.
        //
        // The critical points will still need to do this 16x subdivison, but they furthermore must also respect min
        // segment width.

        auto const log2_decimation_denominator = 4;
        auto const log2_width = log2_domain_max - log2_decimation_denominator;
        auto const decimated_segment_count = 1 << log2_decimation_denominator;

        auto const domain_left = real_t{0};
        auto const domain_right = static_cast<real_t>(domain_max);
        auto const domain_width = domain_right - domain_left;
        auto const segment_width = domain_width / decimated_segment_count;

        auto left = sample_target_function(domain_left);
        for (auto segment = 0; segment < decimated_segment_count; ++segment)
        {
            auto const right = sample_target_function(left.x + segment_width);

            workspace.refinement_pool.push(interval_factory.create(sample_target_function,
                subdomain_t{
                    .left = left,
                    .midpoint = sample_target_function(std::midpoint(left.x, right.x)),
                    .right = right,
                    .log2_width = log2_width,
                }));

            left = right;
        }

        return typename typestate_t::next_t{workspace};
    }
};

template <std::floating_point real_t, typename typestate_t, typename subdivider_t, typename convergence_test_t,
    int_t max_segment_count>
struct refiner_t
{
    using interval_t = subdivider_t::interval_t;

    convergence_test_t requires_subdivision;
    subdivider_t subdivide;

    auto operator()(typestate_t state, auto const& sample_target_function) -> typename typestate_t::next_t
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

        // sort by origin
        std::ranges::sort(completed_intervals, std::ranges::less{},
            [](auto const& interval) noexcept { return interval.subdomain.left.x; });

        return typename typestate_t::next_t{workspace};
    }
};

template <typename typestate_t, int_t domain_max> struct assembler_t
{
    template <typename spline_t> auto operator()(typestate_t state, spline_t& spline) const -> void
    {
        auto& workspace = state.workspace;
        assert(workspace.refinement_pool.empty());

        // extract components
        using segment_locator_t = spline_t::segment_locator_t;
        using segments_t = spline_t::segments_t;
        using segment_t = spline_t::segment_t;
        using x_t = segment_t::x_t;

        auto const segment_count = int_cast<int_t>(std::size(workspace.completed_intervals));
        assert(segment_locator_t::max_segment_count >= segment_count);
        static_assert(segment_locator_t::total_key_count + 1 == spline_t::max_segments);

        segments_t segments;
        using sorted_keys_t = std::array<x_t, segment_locator_t::total_key_count>;
        sorted_keys_t sorted_keys;

        segments[0] = workspace.completed_intervals[0].segment;
        for (auto segment_index = 1; segment_index < segment_count; ++segment_index)
        {
            auto const& interval = workspace.completed_intervals[segment_index];
            segments[segment_index] = interval.segment;
            sorted_keys[segment_index - 1] = to_fixed<x_t>(interval.subdomain.left.x);
        }
        workspace.completed_intervals.clear();

        auto const x_max = x_t{domain_max};
        for (auto key_padding_index = segment_count; key_padding_index < segment_locator_t::total_key_count;
            ++key_padding_index)
        {
            sorted_keys[key_padding_index] = x_max;
        }

        static_assert(std::same_as<typename segments_t::value_type, typename spline_t::segment_t>);
        static_assert(std::same_as<segment_locator_t, typename spline_t::segment_locator_t>);
        static_assert(std::same_as<typename segment_locator_t::x_t, x_t>);

        auto const segment_locator = segment_locator_t{sorted_keys, x_max, segment_count};
        spline = spline_t{segment_locator, segments};
    }
};

// runs subdivision loop over queue and completed segments
template <std::floating_point real_t, typename spline_t, typename typestates_t, typename refinement_pool_t,
    typename refinement_pool_seeder_t, typename refiner_t, typename assembler_t, int_t max_segment_count,
    int_t domain_max>
class spline_generator_t
{
public:
    constexpr spline_generator_t() : seed_refinement_pool_{}, refine_{}, assemble_{}, workspace_{} {}

    constexpr spline_generator_t(refinement_pool_seeder_t seed_refinement_pool, refiner_t refine, assembler_t assemble)
        : seed_refinement_pool_{std::move(seed_refinement_pool)}, refine_{std::move(refine)},
          assemble_{std::move(assemble)}, workspace_{}
    {}

    constexpr auto operator()(auto& spline, auto target_function) -> void
    {
        assert(workspace_.empty());

        workspace_.clear();
        workspace_.completed_intervals.reserve(max_segment_count);
        workspace_.refinement_pool.reserve(max_segment_count);

        auto sample_target_function = function_sampler_t{std::move(target_function)};

        auto const seeded_state
            = seed_refinement_pool_(typename typestates_t::unseeded_t{workspace_}, sample_target_function);
        auto const refined_state = refine_(seeded_state, sample_target_function);
        assemble_(refined_state, spline);

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
    using real_t = std::float128_t;
#else
    using real_t = float_t;
#endif

    using x_t = fixed_t<uint64_t, 40>;
    using y_t = fixed_t<uint64_t, 48>;
    using normalized_t = fixed_t<uint64_t, 64>;

    using coeff_t = fixed_t<int64_t, 47>;

    constexpr auto depth_max = 4;
    constexpr auto max_segment_count = 1 << (depth_max * 2);
    constexpr auto log2_domain_max = 8;
    constexpr auto domain_max = 1 << log2_domain_max;
    constexpr auto log2_min_width = -16;
    constexpr auto log2_width_bit_count = 5;
    constexpr auto global_tolerance = 1e-10; // should max against integral

#if 1
    static auto const min_width = std::ldexp(real_t{1}, log2_min_width);
    using error_norm_t = error_norms::first_order_relative_t<real_t, error_norms::logsumexp_floor_t<real_t>>;
    auto const error_norm = error_norm_t{.primal_floor = min_width, .tangent_floor = min_width};
#else
    using error_norm_t = error_norms::absolute_t;
    auto const error_norm = error_norm_t{};
#endif

#if 0
    // with the steepest log1p we support with 16 initial segments, 181.625x this always produces segments with
    // error > 1e-5 near about x=50 unless you make it basically flat already
    using weight_function_t = weight_functions::hyperbolic_decay_t<real_t>;
    auto const weight_function = weight_function_t{.halflife = 0.5};
#else
    using weight_function_t = weight_functions::uniform_t<real_t>;
    auto const weight_function = weight_function_t{};
#endif

    using polynomial_evaluator_t = polynomial_evaluator_t<mac_t{}>;
    using packed_segment_t = static_packed_segment_t<coeff_t, log2_width_bit_count>;
    using segment_t = segment_t<x_t, y_t, coeff_t, normalized_t, packed_segment_t, polynomial_evaluator_t{}>;
    using subdomain_t = subdomain_t<real_t>;
    using interval_t = interval_t<real_t, segment_t>;
    using refinement_pool_t = priority_queue_t<std::vector<interval_t>, interval_priority_less_t>;
    using node_generator_t = node_generators::equioscillation_t<real_t>;
    using residual_estimator_t = residual_estimator_t<real_t, node_generator_t, error_norm_t, weight_function_t>;
    using segment_derivative_t = segment_derivative_t<real_t>;
    using hermite_converter_t = hermite_converter_t<coeff_t>;
    using segment_factory_t = segment_factory_t<real_t, segment_t, segment_derivative_t, hermite_converter_t>;
    using approximant_t = approximant_t<real_t, segment_t, segment_derivative_t>;
    using interval_factory_t = interval_factory_t<interval_t, approximant_t, segment_factory_t, residual_estimator_t>;
    using bisection_t = bisection_t<subdomain_t>;
    using bisector_t = bisector_t<bisection_t>;
    using convergence_test_t = convergence_test_t<real_t, log2_min_width>;
    using subdivider_t = subdivider_t<real_t, bisector_t, interval_factory_t>;
    using segment_locator_t = segment_locator_t<x_t, depth_max>;
    using spline_t = spline_t<segment_t, segment_locator_t>;
    using workspace_t = workspace_t<interval_t, max_segment_count>;
    using typestates_t = typestates_t<workspace_t>;
    using assembler_t = assembler_t<typestates_t::refined_t, domain_max>;
    using refiner_t = refiner_t<real_t, typestates_t::seeded_t, subdivider_t, convergence_test_t, max_segment_count>;
    using refinement_pool_seeder_t
        = refinement_pool_seeder_t<real_t, typestates_t::unseeded_t, interval_factory_t, log2_domain_max, domain_max>;
    using spline_generator_t = spline_generator_t<real_t, spline_t, typestates_t, refinement_pool_t,
        refinement_pool_seeder_t, refiner_t, assembler_t, max_segment_count, domain_max>;

    auto const estimate_residual = residual_estimator_t{
        .generate_nodes = {},
        .measure_error = error_norm,
        .apply_weight = weight_function,
    };

    auto const interval_factory = interval_factory_t{
        .create_segment = {},
        .estimate_residual = estimate_residual,
    };

    auto spline_generator = spline_generator_t
    {
        refinement_pool_seeder_t{.interval_factory = interval_factory},
        refiner_t
        {
            .requires_subdivision = convergence_test_t{.global_tolerance=global_tolerance},
            .subdivide = subdivider_t
            {
                .bisect = bisector_t{},
                .interval_factory = interval_factory,
            },
        },
        assembler_t{}
    };

    auto const target_function = [](auto x) static noexcept -> decltype(x) {
        using std::log1p;
        return 181.625 * log1p(x);
    };

    auto spline = spline_t{};
    spline_generator(spline, std::ref(target_function));

#if 1
    auto x_fixed = x_t{0};
    auto const sample_count = 255;
    auto const dx = x_t{domain_max} / sample_count;
    for (auto sample = 0; sample < sample_count; ++sample, x_fixed += dx)
    {
        using std::log1p;

        auto const x_real = from_fixed<real_t>(x_fixed);

        auto const expected_y = target_function(x_real);
        auto const actual_y = from_fixed<real_t>(spline(x_fixed));

        auto const difference = actual_y - expected_y;
        ASSERT_LT(abs(difference), 1e-5);

        std::cout << std::setprecision(4) << "x = " << static_cast<long double>(x_real)
                  << ", log1p(x) = " << static_cast<long double>(expected_y)
                  << ", y_actual = " << static_cast<long double>(actual_y)
                  << ", Δy = " << static_cast<long double>(difference) << std::endl;
    }
    std::cout << std::endl;
#endif
}

} // namespace
} // namespace spline
} // namespace crv
