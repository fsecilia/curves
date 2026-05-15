// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/test/test.hpp>

#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/construction/approximant.hpp>
#include <crv/math/spline/construction/cubic.hpp>
#include <crv/math/spline/construction/error_norm.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/hermite_converter.hpp>
#include <crv/math/spline/construction/node_generator.hpp>
#include <crv/math/spline/construction/residual_estimator.hpp>
#include <crv/math/spline/construction/segment_factory.hpp>
#include <crv/math/spline/construction/weight_function.hpp>
#include <crv/math/spline/segment.hpp>
#include <crv/math/spline/segment_locator.hpp>
#include <crv/math/spline/spline.hpp>
#include <crv/priority_queue.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <stdfloat>

namespace crv {
namespace spline {
namespace {

/// geometry of a refinement subdomain
///
/// This type brackets the subdomain [left, right]. It includes log2_width and samples for left, right, and midpoint.
template <std::floating_point scalar_t> struct subdomain_t
{
    using function_sample_t = function_sample_t<scalar_t>;

    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;
    int_t log2_width;

    constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
};

/// unit of work over a subdomain
template <std::floating_point t_scalar_t> struct interval_t
{
    using scalar_t = t_scalar_t;

    using subdomain_t = subdomain_t<scalar_t>;
    using residual_t = residual_t<scalar_t>;
    using cubic_t = cubic_t<scalar_t>;

    subdomain_t subdomain;
    cubic_t cubic;
    residual_t residual;

    constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
};

/// orders by residual.weighted_error then domain.left.x
struct interval_priority_less_t
{
    template <typename interval_t>
    constexpr auto operator()(interval_t const& lhs, interval_t const& rhs) const noexcept -> bool
    {
        using std::isfinite;
        assert(isfinite(lhs.residual.weighted_error));
        assert(isfinite(lhs.subdomain.left.x));
        assert(isfinite(rhs.residual.weighted_error));
        assert(isfinite(rhs.subdomain.left.x));

        // tie applies lexicographical compare
        return std::tie(lhs.residual.weighted_error, lhs.subdomain.left.x)
            < std::tie(rhs.residual.weighted_error, rhs.subdomain.left.x);
    }
};

/// constructs intervals from subdomains
template <typename t_interval_t, typename approximant_t, typename hermite_converter_t, typename residual_estimator_t>
struct interval_factory_t
{
    using interval_t = t_interval_t;

    using scalar_t = interval_t::scalar_t;
    using subdomain_t = subdomain_t<scalar_t>;

    [[no_unique_address]] hermite_converter_t hermite_converter;
    residual_estimator_t estimate_residual;

    constexpr auto create(auto const& sample_target_function, subdomain_t const& subdomain) const noexcept -> interval_t
    {
        using x_t = approximant_t::x_t;

        auto const x0 = to_fixed<x_t>(subdomain.left.x);

        // convert from spline-global dy/dx to segment-local dy/dt via chain rule
        auto const dx_dt = std::ldexp(1.0, static_cast<int>(subdomain.log2_width));
        auto const local_left_y = jet_t{subdomain.left.y.f, subdomain.left.y.df * dx_dt};
        auto const local_right_y = jet_t{subdomain.right.y.f, subdomain.right.y.df * dx_dt};
        auto const cubic = hermite_converter(local_left_y, local_right_y);

        return {
            .subdomain = subdomain,
            .cubic = cubic,
            .residual = estimate_residual(sample_target_function,
                approximant_t{
                    .cubic = cubic,
                    .x0 = x0,
                    .log2_width = subdomain.log2_width,
                },
                subdomain.left.x, subdomain.midpoint.x, subdomain.right.x),
        };
    }
};

/// fixed-point cubic spline segment packed into half a cache line
template <is_fixed t_x_t, typename t_packed_segment_t, typename t_segment_unpacker_t, typename t_segment_evaluator_t>
class alignas(32) segment_t
{
public:
    using x_t = t_x_t;
    using packed_segment_t = t_packed_segment_t;
    using segment_unpacker_t = t_segment_unpacker_t;
    using segment_evaluator_t = t_segment_evaluator_t;

    using y_t = segment_evaluator_t::y_t;

    constexpr segment_t() noexcept : packed_segment_{} {}

    explicit constexpr segment_t(packed_segment_t packed_segment) noexcept : packed_segment_{packed_segment}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_t>);

        // this type must be aligned to at least half a cache line; during prod it must be exactly 32, but during
        // testing, it may be overaligned
        static_assert(alignof(segment_t) >= 32);
    }

    constexpr auto operator()(x_t dx) const noexcept -> y_t
    {
        return y_t::convert(evaluate_segment(unpack_segment(packed_segment_), dx));
    }

private:
    [[no_unique_address]] segment_unpacker_t unpack_segment;
    [[no_unique_address]] segment_evaluator_t evaluate_segment;
    packed_segment_t packed_segment_;
};

/// creates final segment from its cubic and width
template <typename t_segment_t, typename t_segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;
    using segment_packer_t = t_segment_packer_t;

    using scalar_t = segment_packer_t::scalar_t;
    using cubic_t = segment_packer_t::cubic_t;
    using function_sample_t = function_sample_t<scalar_t>;

    [[no_unique_address]] segment_packer_t pack_segment;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> segment_t
    {
        return segment_t{pack_segment(cubic, log2_width)};
    }
};

/// mutable state for adaptive mesh refinement
template <typename interval_t, int_t max_segments> struct workspace_t
{
    std::vector<interval_t> completed_intervals;
    priority_queue_t<std::vector<interval_t>, interval_priority_less_t> refinement_pool;

    constexpr auto clear() noexcept -> void
    {
        completed_intervals.clear();
        refinement_pool.clear();
    }

    constexpr auto empty() const noexcept -> bool { return completed_intervals.empty() && refinement_pool.empty(); }

    constexpr workspace_t()
    {
        completed_intervals.reserve(max_segments);
        refinement_pool.reserve(max_segments);
    }
};

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

/// decides if an interval should subdivide
template <std::floating_point scalar_t, int_t log2_min_width> struct convergence_test_t
{
    static constexpr auto relative_noise_margin = std::numeric_limits<scalar_t>::epsilon() * scalar_t{64};
    scalar_t global_tolerance;

    constexpr auto operator()(auto const& interval) const noexcept -> bool
    {
        auto const noise_floor = interval.residual.scale * relative_noise_margin;
        auto const local_tolerance = max(global_tolerance, noise_floor);
        auto const can_subdivide
            = interval.subdomain.log2_width > log2_min_width && interval.residual.metric_error >= local_tolerance;
        auto const should_subdivide = interval.residual.metric_error > global_tolerance;
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
template <std::floating_point scalar_t, typename bisector_t, typename interval_factory_t> struct subdivider_t
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

template <typename t_interval_t, typename t_segment_t, typename t_segment_packer_t, typename t_segment_unpacker_t,
    typename t_field_packer_t, typename saturating_shifter_t>
struct tangent_extender_t
{
    using interval_t = t_interval_t;
    using segment_t = t_segment_t;
    using segment_packer_t = t_segment_packer_t;
    using segment_unpacker_t = t_segment_unpacker_t;
    using field_packer_t = t_field_packer_t;

    using x_t = segment_t::x_t;
    using scalar_t = segment_packer_t::scalar_t;
    using cubic_t = segment_packer_t::cubic_t;

    [[no_unique_address]] segment_packer_t pack_segment;
    [[no_unique_address]] segment_unpacker_t unpack_segment;
    [[no_unique_address]] field_packer_t pack_field;
    [[no_unique_address]] saturating_shifter_t saturating_shifter;

    constexpr auto operator()(interval_t const& interval, segment_t const& segment) const noexcept -> segment_t
    {
        auto const log2_x_max
            = sizeof(typename x_t::value_t) * CHAR_BIT - x_t::frac_bits - is_signed_v<typename x_t::value_t>;

        auto const t = scalar_t{1};
        auto const dt_dx = std::ldexp(scalar_t{1}, int_cast<int>(log2_x_max - interval.subdomain.log2_width));
        auto const dy_dx_extended_segment = interval.cubic(jet_t{t, dt_dx}).df;

        // find fixed y at dx = width
        auto const final_segment_width = x_t{1} << interval.subdomain.log2_width;
        auto const y1_actual = segment(final_segment_width);

        // pack proto and unpack to find shift
        auto y = from_fixed<scalar_t>(y1_actual);
        auto tangent_cubic = cubic_t{0, 0, dy_dx_extended_segment, y};
        auto packed_segment = pack_segment(tangent_cubic, log2_x_max);
        auto const unpacked_segment = unpack_segment(packed_segment);
        auto const c3_shift = unpacked_segment[3].shift;

        // solve target mantissa directly via renormalization
        auto const y1_as_signed = std::saturating_cast<mantissa_t>(y1_actual.value);
        auto const target_mantissa = saturating_shifter.shift(y1_as_signed, c3_shift);

        // repack new c3
        packed_segment[fields_per_segment - 1]
            = pack_field(unpacked_field_t{.mantissa = target_mantissa, .shift = c3_shift}, final_layout);
        return segment_t{packed_segment};
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
template <std::floating_point scalar_t, typename typestate_t, typename interval_factory_t, int_t log2_domain_max,
    int_t domain_max>
struct refinement_pool_seeder_t
{
    interval_factory_t interval_factory;

    constexpr auto operator()(typestate_t state, auto const& sample_target_function) const ->
        typename typestate_t::next_t
    {
        using subdomain_t = subdomain_t<scalar_t>;

        auto& workspace = state.workspace;
        assert(workspace.empty());

        auto left = sample_target_function(scalar_t{0});
        auto const right = sample_target_function(static_cast<scalar_t>(domain_max));

        workspace.refinement_pool.push(interval_factory.create(sample_target_function,
            subdomain_t{
                .left = left,
                .midpoint = sample_target_function(std::midpoint(left.x, right.x)),
                .right = right,
                .log2_width = log2_domain_max,
            }));

        return typename typestate_t::next_t{workspace};
    }
};

template <std::floating_point scalar_t, typename typestate_t, typename subdivider_t, typename convergence_test_t,
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

template <typename typestate_t, typename t_segment_factory_t, typename t_tangent_extender_t, int_t domain_max>
struct assembler_t
{
    using segment_factory_t = t_segment_factory_t;
    using segment_t = segment_factory_t::segment_t;
    using tangent_extender_t = t_tangent_extender_t;

    [[no_unique_address]] segment_factory_t segment_factory;
    [[no_unique_address]] tangent_extender_t extend_tangent;

    template <typename spline_t> auto operator()(typestate_t state, spline_t& spline) const -> void
    {
        auto& workspace = state.workspace;
        assert(workspace.refinement_pool.empty());

        // extract components
        using segment_locator_t = spline_t::segment_locator_t;
        using segments_t = spline_t::segments_t;
        using x_t = segment_t::x_t;

        auto const segment_count = int_cast<int_t>(std::size(workspace.completed_intervals));
        assert(segment_locator_t::max_segment_count >= segment_count);
        static_assert(segment_locator_t::total_key_count + 1 == spline_t::max_segment_count);

        segments_t segments;
        using sorted_keys_t = std::array<x_t, segment_locator_t::total_key_count>;
        sorted_keys_t sorted_keys;

        segments[0] = make_segment(workspace.completed_intervals[0]);
        for (auto segment_index = 1; segment_index < segment_count; ++segment_index)
        {
            auto const& interval = workspace.completed_intervals[segment_index];
            segments[segment_index] = make_segment(interval);
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
        auto const final_interval = workspace.completed_intervals[segment_count - 1];
        auto const final_segment = segments[segment_count - 1];
        auto const extended_tangent = extend_tangent(final_interval, final_segment);

        workspace.completed_intervals.clear();

        auto const segment_locator = segment_locator_t{sorted_keys, x_max, segment_count};
        spline = spline_t{segment_locator, segments, extended_tangent};
    }

private:
    constexpr auto make_segment(auto const& interval) const noexcept -> segment_t
    {
        return segment_factory(interval.cubic, interval.subdomain.log2_width);
    }
};

// runs subdivision loop over queue and completed segments
template <std::floating_point scalar_t, typename spline_t, typename typestates_t, typename refinement_pool_t,
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
    using scalar_t = std::float128_t;
#else
    using scalar_t = float_t;
#endif

    using x_t = fixed_t<uint64_t, 42>;
    using y_t = fixed_t<uint64_t, 48>;

    constexpr auto depth_max = 4;
    constexpr auto max_segment_count = 1 << (depth_max * 2);
    constexpr auto log2_domain_max = 8;
    constexpr auto domain_max = 1 << log2_domain_max;
    constexpr auto log2_min_width = -16;
    constexpr auto global_tolerance = 1e-10; // should max against integral

#if 1
    static auto const min_width = std::ldexp(scalar_t{1}, log2_min_width);
    using error_norm_t = error_norms::first_order_relative_t<scalar_t, error_norms::logsumexp_floor_t<scalar_t>>;
    auto const error_norm = error_norm_t{.primal_floor = min_width, .tangent_floor = min_width};
#else
    using error_norm_t = error_norms::absolute_t;
    auto const error_norm = error_norm_t{};
#endif

#if 1
    // with the steepest log1p we support with 16 initial segments, 181.625x this always produces segments with
    // error > 1e-5 near about x=50 unless you make it basically flat already
    using weight_function_t = weight_functions::hyperbolic_decay_t<scalar_t>;
    auto const weight_function = weight_function_t{.halflife = 0.5};
#else
    using weight_function_t = weight_functions::uniform_t<scalar_t>;
    auto const weight_function = weight_function_t{};
#endif

    using segment_evaluator_t = segment_evaluator_t<x_t, y_t>;
    using segment_unpacker_t = segment_unpacker_t<field_unpacker_t>;
    using segment_t = segment_t<x_t, packed_segment_t, segment_unpacker_t, segment_evaluator_t>;
    using subdomain_t = subdomain_t<scalar_t>;
    using interval_t = interval_t<scalar_t>;
    using refinement_pool_t = priority_queue_t<std::vector<interval_t>, interval_priority_less_t>;
    using node_generator_t = node_generators::equioscillation_t<scalar_t>;
    using residual_estimator_t = residual_estimator_t<scalar_t, node_generator_t, error_norm_t, weight_function_t>;
    using hermite_converter_t = hermite_converter_t<scalar_t>;
    using approximant_t = approximant_t<scalar_t, x_t>;
    using interval_factory_t = interval_factory_t<interval_t, approximant_t, hermite_converter_t, residual_estimator_t>;
    using bisection_t = bisection_t<subdomain_t>;
    using bisector_t = bisector_t<bisection_t>;
    using convergence_test_t = convergence_test_t<scalar_t, log2_min_width>;
    using subdivider_t = subdivider_t<scalar_t, bisector_t, interval_factory_t>;
    using segment_locator_t = segment_locator_t<x_t, depth_max>;
    using spline_t = spline_t<segment_t, segment_locator_t>;
    using workspace_t = workspace_t<interval_t, max_segment_count>;
    using typestates_t = typestates_t<workspace_t>;
    using float_extractor_t = float_extractor_t<scalar_t>;
    using scaled_int_t = float_extractor_t::scaled_int_t;
    using exponent_renormalizer_t = exponent_renormalizer_t<final_layout_min_shift, final_layout_max_shift>;
    using segment_builder_t = segment_builder_t<scaled_int_t, x_t, y_t, exponent_renormalizer_t>;
    using builder_factory_t = builder_factory_t<segment_builder_t>;
    using segment_packer_t = segment_packer_t<float_extractor_t, field_packer_t, builder_factory_t, log2_min_width>;
    using segment_factory_t = segment_factory_t<segment_t, segment_packer_t>;
    using saturating_shifter_t = saturating_shifter_t<>;
    using tangent_extender_t = tangent_extender_t<interval_t, segment_t, segment_packer_t, segment_unpacker_t,
        field_packer_t, saturating_shifter_t>;
    using assembler_t = assembler_t<typestates_t::refined_t, segment_factory_t, tangent_extender_t, domain_max>;
    using refiner_t = refiner_t<scalar_t, typestates_t::seeded_t, subdivider_t, convergence_test_t, max_segment_count>;
    using refinement_pool_seeder_t
        = refinement_pool_seeder_t<scalar_t, typestates_t::unseeded_t, interval_factory_t, log2_domain_max, domain_max>;
    using spline_generator_t = spline_generator_t<scalar_t, spline_t, typestates_t, refinement_pool_t,
        refinement_pool_seeder_t, refiner_t, assembler_t, max_segment_count, domain_max>;

    auto const estimate_residual = residual_estimator_t{
        .generate_nodes = {},
        .measure_error = error_norm,
        .apply_weight = weight_function,
    };

    auto const interval_factory = interval_factory_t{
        .hermite_converter = {},
        .estimate_residual = estimate_residual,
    };

    auto generate_spline = spline_generator_t
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
        return 18.1625 * log1p(x);
    };

    auto spline = spline_t{};
    generate_spline(spline, std::ref(target_function));

#if 1
    auto x_fixed = x_t{0};
    auto const sample_count = 255;
    auto const dx = x_t{domain_max} / sample_count;
    for (auto sample = 0; sample < sample_count; ++sample, x_fixed += dx)
    {
        using std::log1p;

        auto const x_real = from_fixed<scalar_t>(x_fixed);

        auto const expected_y = target_function(x_real);
        auto const actual_y = from_fixed<scalar_t>(spline(x_fixed));

        auto const difference = actual_y - expected_y;
        ASSERT_LT(abs(difference), 1.6e-5); //

        std::cout << std::setprecision(4) << "x = " << static_cast<float_max_t>(x_real)
                  << ", log1p(x) = " << static_cast<float_max_t>(expected_y)
                  << ", y_actual = " << static_cast<float_max_t>(actual_y)
                  << ", Δy = " << static_cast<float_max_t>(difference) << std::endl;
    }
    std::cout << std::endl;
#endif
}

} // namespace
} // namespace spline
} // namespace crv
