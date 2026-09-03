// SPDX-License-Identifier: MIT

/// \file
/// \brief tests how much critical points for limiters affects amr
///
/// This test runs amr over a limted curve with and without critical points at the edges of a c1 and c2 transition over
/// various transition widths. The goal is to test both accuracy and the number of segments generated. The result is
/// critical points tend to not be worth the cost of the inversion machinery to find them for c1 and c2 transitions.
/// For features smaller than the min segment width, critical points tend to help immensely, but the curves we construct
/// are unlikely to have such features by definition.
///
/// The recommendation after running this test is not to bother with the inversion if the only reason is joints at
/// c1|c2 transitions.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/model/curves/power_law.hpp>
#include <crv/model/shaping/limited_curve.hpp>
#include <crv/model/shaping/transforms/limiter.hpp>
#include <crv/model/shaping/transitions/smootherstep.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using policy_t = default_spline_policy_t<scalar_t, prod_pipeline_config_t>;
using x_t = policy_t::x_t;
using spline_factory_t = spline_factory_t<policy_t, spline_generator_factory_t<policy_t>>;
using spline_t = spline_factory_t::spline_t;

struct identity_gain_curve_t
{
    using scalar_t = crv::float_t;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return input;
    }
};

struct limiter_width_case_t
{
    char const* name;
    scalar_t support_width;
};

constexpr auto limiter_width_cases = std::array{
    limiter_width_case_t{"wide", scalar_t{8}},
    limiter_width_case_t{"moderate", scalar_t{0.25}},
    limiter_width_case_t{"narrow", scalar_t{0.01}},
    limiter_width_case_t{"refinement_floor_scale", scalar_t{0.002}},
    limiter_width_case_t{"below_refinement_floor", scalar_t{0.0001}},
};

struct support_t
{
    scalar_t lower;
    scalar_t upper;
};

struct experiment_result_t
{
    spline_generation_result_t<x_t> construction;
    int_t segment_count{};
    scalar_t max_error{};
};

/// exercises production AMR against an output-limited gain curve
struct limiter_amr_validation_test_t : Test
{
    static constexpr auto bound = scalar_t{73};
    static constexpr auto tolerance = policy_t::spline_gain_tolerance;
    static constexpr auto domain_sample_count = int_t{4096};
    static constexpr auto local_sample_count = int_t{4096};

    static auto evaluate_spline(spline_t const& spline, x_t input) noexcept -> scalar_t
    {
        auto hint = spline_t::hint_t{};
        return from_fixed<scalar_t>(spline.evaluate(input, hint));
    }

    static auto half_width_for_support_width(scalar_t support_width) noexcept -> scalar_t
    {
        return std::asinh(support_width / (scalar_t{2} * bound));
    }

    template <typename transition_t>
    static auto delta_y_for_support_width(scalar_t support_width, transition_t const& transition) noexcept -> scalar_t
    {
        auto const half_width = half_width_for_support_width(support_width);
        auto const half_integral = transition.antiderivative(scalar_t{0.5});
        return bound * -std::expm1(-scalar_t{2} * half_width * half_integral);
    }

    static auto support_for_width(scalar_t support_width) noexcept -> support_t
    {
        auto const half_width = half_width_for_support_width(support_width);
        return {
            .lower = bound * std::exp(-half_width),
            .upper = bound * std::exp(half_width),
        };
    }

    template <typename transition_t>
    using limited_identity_curve_t
        = shaping::limited_curve_t<shaping::transforms::upper_limiter_t<scalar_t, transition_t>, identity_gain_curve_t>;

    template <typename transition_t>
    static auto make_curve(scalar_t support_width, transition_t transition) -> limited_identity_curve_t<transition_t>
    {
        using limiter_t = shaping::transforms::upper_limiter_t<scalar_t, transition_t>;
        auto const delta_y = delta_y_for_support_width(support_width, transition);
        auto limiter = limiter_t::make(bound, delta_y, transition).value();
        return limited_identity_curve_t<transition_t>{std::move(limiter), identity_gain_curve_t{}};
    }

    static auto sample_error_at(spline_t const& spline, auto const& target, scalar_t input) noexcept -> scalar_t
    {
        auto const fixed_input = to_fixed<x_t>(input);
        auto const representable_input = from_fixed<scalar_t>(fixed_input);
        return abs(target.gain(representable_input) - evaluate_spline(spline, fixed_input));
    }

    static auto max_approximation_error(spline_t const& spline, auto const& target, support_t support) noexcept
        -> scalar_t
    {
        auto max_error = scalar_t{};
        auto const domain_end = scalar_t{policy_t::domain_end};
        for (auto sample = int_t{0}; sample <= domain_sample_count; ++sample)
        {
            auto const input = domain_end * static_cast<scalar_t>(sample) / static_cast<scalar_t>(domain_sample_count);
            max_error = std::max(max_error, sample_error_at(spline, target, input));
        }

        auto const support_width = support.upper - support.lower;
        auto const local_begin = std::max(scalar_t{0}, support.lower - support_width);
        auto const local_end = std::min(domain_end, support.upper + support_width);
        for (auto sample = int_t{0}; sample <= local_sample_count; ++sample)
        {
            auto const fraction = static_cast<scalar_t>(sample) / static_cast<scalar_t>(local_sample_count);
            auto const input = local_begin + (local_end - local_begin) * fraction;
            max_error = std::max(max_error, sample_error_at(spline, target, input));
        }
        return max_error;
    }

    template <typename transition_t>
    static auto run_case(limiter_width_case_t width_case, bool include_support_points) -> experiment_result_t
    {
        auto const transition = transition_t{};
        auto const curve = make_curve(width_case.support_width, transition);
        auto const target = gain_curve_target_t{curve};
        auto const support = support_for_width(width_case.support_width);
        auto critical_points = std::vector<x_t>{};
        if (include_support_points) { critical_points = {to_fixed<x_t>(support.lower), to_fixed<x_t>(support.upper)}; }

        auto spline = spline_t{};
        auto const construction = spline_factory_t{}(spline, target, tolerance, std::move(critical_points));
        if (!construction) return {.construction = construction};

        return {
            .construction = construction,
            .segment_count = spline.segment_locator.segment_count(),
            .max_error = max_approximation_error(spline, target, support),
        };
    }

    static auto print_result(std::string_view continuity, limiter_width_case_t width_case,
        experiment_result_t const& without_points, experiment_result_t const& with_points) -> void
    {
        auto print_one = [](std::string_view label, experiment_result_t const& result) -> void {
            std::cout << label << "={success=" << static_cast<bool>(result.construction);
            if (result.construction)
            {
                std::cout << ", segments=" << result.segment_count << ", max_error=" << std::setprecision(12)
                          << result.max_error;
            }
            else
            {
                auto const& error = *result.construction.error;
                std::cout << ", reason=" << static_cast<int_t>(error.reason) << ", interval=["
                          << from_fixed<scalar_t>(error.left) << ", " << from_fixed<scalar_t>(error.right) << ']';
            }
            std::cout << '}';
        };

        std::cout << continuity << ' ' << width_case.name << " width=" << std::setprecision(12)
                  << width_case.support_width
                  << " min_width=" << from_fixed<scalar_t>(policy_t::subdivision_predicate_t::min_width) << ' ';
        print_one("without_support_points", without_points);
        std::cout << ' ';
        print_one("with_support_points", with_points);
        std::cout << '\n';
    }

    template <typename transition_t>
    static auto run_comparison(std::string_view continuity, limiter_width_case_t width_case) -> bool
    {
        auto const without_points = run_case<transition_t>(width_case, false);
        auto const with_points = run_case<transition_t>(width_case, true);
        print_result(continuity, width_case, without_points, with_points);
        return without_points.construction && with_points.construction;
    }
};

struct limiter_amr_c1_validation_test_t : limiter_amr_validation_test_t, WithParamInterface<limiter_width_case_t>
{};

TEST_P(limiter_amr_c1_validation_test_t, constructs_with_and_without_support_critical_points)
{
    EXPECT_TRUE((run_comparison<shaping::transitions::smoothstep_t>("C1", GetParam())));
}

INSTANTIATE_TEST_SUITE_P(c1_widths, limiter_amr_c1_validation_test_t, ValuesIn(limiter_width_cases),
    test_name_generator_t<limiter_width_case_t>{});

struct limiter_amr_c2_validation_test_t : limiter_amr_validation_test_t, WithParamInterface<limiter_width_case_t>
{};

TEST_P(limiter_amr_c2_validation_test_t, constructs_with_and_without_support_critical_points)
{
    EXPECT_TRUE((run_comparison<shaping::transitions::smootherstep_t>("C2", GetParam())));
}

INSTANTIATE_TEST_SUITE_P(c2_widths, limiter_amr_c2_validation_test_t, ValuesIn(limiter_width_cases),
    test_name_generator_t<limiter_width_case_t>{});

/// test-only composition that absorbs sanctioned positive overflow before entering the production limiter
struct overflow_absorbing_power_law_t
{
    using transition_t = shaping::transitions::smootherstep_t;
    using limiter_t = shaping::transforms::upper_limiter_t<scalar_t, transition_t>;
    using nested_curve_t = model::curves::power_law_t::evaluator_t<scalar_t>;
    using jet_t = crv::jet_t<scalar_t>;

    scalar_t bound;
    limiter_t limiter;
    nested_curve_t nested_curve;

    [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t
    {
        auto const nested = nested_curve(input);
        if (nested == std::numeric_limits<scalar_t>::infinity()) return bound;
        assert(std::isfinite(nested) && nested >= scalar_t{0});
        return limiter(nested);
    }

    [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t
    {
        auto const nested = nested_curve(primal(input));
        if (nested == std::numeric_limits<scalar_t>::infinity()) return jet_t{bound};
        assert(std::isfinite(nested) && nested >= scalar_t{0});
        return limiter.apply(nested_curve, input);
    }
};

struct limiter_amr_power_law_validation_test_t : limiter_amr_validation_test_t
{
    static constexpr auto power_law_bound = scalar_t{4};
    static constexpr auto transition_center = scalar_t{16};
    static constexpr auto support_width = scalar_t{0.25};
    static constexpr auto power = scalar_t{300};

    static auto make_overflowing_curve() -> overflow_absorbing_power_law_t
    {
        auto const transition = overflow_absorbing_power_law_t::transition_t{};
        auto const half_integral = transition.antiderivative(scalar_t{0.5});
        auto const half_width = power * std::asinh(support_width / (scalar_t{2} * transition_center));
        auto const delta_y = power_law_bound * -std::expm1(-scalar_t{2} * half_width * half_integral);
        auto limiter = overflow_absorbing_power_law_t::limiter_t::make(power_law_bound, delta_y, transition).value();
        auto const unit_speed = transition_center / std::pow(power_law_bound, scalar_t{1} / power);
        auto nested_curve = overflow_absorbing_power_law_t::nested_curve_t{
            model::curves::power_law_t::params_t<scalar_t>{.unit_speed = unit_speed, .power = power}};
        return {.bound = power_law_bound, .limiter = std::move(limiter), .nested_curve = std::move(nested_curve)};
    }
};

TEST_F(limiter_amr_power_law_validation_test_t, sanctioned_positive_overflow_is_absorbed_before_limiter_arithmetic)
{
    auto const curve = make_overflowing_curve();
    auto const input = scalar_t{policy_t::domain_end};
    auto const actual
        = std::tuple{std::isinf(curve.nested_curve(input)), curve(input), curve(jet_t<scalar_t>{input, 1})};
    auto const expected = std::tuple{true, power_law_bound, jet_t<scalar_t>{power_law_bound, 0}};
    EXPECT_EQ(actual, expected);
}

} // namespace
} // namespace crv::spline
