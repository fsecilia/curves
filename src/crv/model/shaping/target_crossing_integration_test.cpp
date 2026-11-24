// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "target_crossing.hpp"
#include <crv/math/inverse.hpp>
#include <crv/model/curves/power_law.hpp>
#include <crv/model/shaping/curve_evaluator.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv::shaping {
namespace {

using scalar_t = float_t;
using comparison_t = crossing_comparison_t;
using outcome_t = crossing_result_t<scalar_t>;
using finite_t = finite_crossing_t<scalar_t>;
using frontier_t = crossing_frontier_t<scalar_t>;
using absent_t = crossing_absent_t;
using error_t = curve_evaluation_error_t;
using evaluation_t = std::expected<scalar_t, error_t>;

constexpr auto infinity = std::numeric_limits<scalar_t>::infinity();
constexpr auto max = std::numeric_limits<scalar_t>::max();
constexpr auto inclusive = comparison_t::inclusive;
constexpr auto strict = comparison_t::strict;

struct target_crossing_integration_test_t : Test
{
    target_crossing_t<try_bisect_first_true_t> sut{{}};

    // exact plateaus followed by an infinite suffix
    struct curve_t
    {
        auto try_evaluate(scalar_t input) const noexcept -> evaluation_t
        {
            if (input < 1.0) return 0.0;
            if (input < 3.0) return 2.0;
            if (input < 5.0) return 4.0;
            return infinity;
        }
    };
    curve_t curve;
};

struct crossing_case_t
{
    char const* name;
    scalar_t low;
    scalar_t high;
    scalar_t target;
    comparison_t comparison;
    outcome_t expected;
};

struct target_crossing_boundaries_test_t : target_crossing_integration_test_t, WithParamInterface<crossing_case_t>
{};

TEST_P(target_crossing_boundaries_test_t, resolves_exact_first_representable_input)
{
    auto const& param = GetParam();
    EXPECT_EQ(sut(param.low, param.high, param.target, param.comparison, curve), param.expected);
}

crossing_case_t const crossing_cases[] = {
    {"finite_inclusive_before_overflow", 0.0, 9.0, 2.0, inclusive, finite_t{1.0}},
    {"finite_strict_before_overflow", 0.0, 9.0, 2.0, strict, finite_t{3.0}},
    {"frontier_inclusive_skips_target", 0.0, 9.0, 6.0, inclusive, frontier_t{5.0}},
    {"frontier_strict_skips_target", 0.0, 9.0, 6.0, strict, frontier_t{5.0}},
    {"last_finite_inclusive", 0.0, 9.0, 4.0, inclusive, finite_t{3.0}},
    {"last_finite_strict", 0.0, 9.0, 4.0, strict, frontier_t{5.0}},
    {"plateau_inclusive", 0.0, 4.0, 2.0, inclusive, finite_t{1.0}},
    {"plateau_strict", 0.0, 4.0, 2.0, strict, finite_t{3.0}},
    {"absent_inclusive", 0.0, 4.0, 6.0, inclusive, absent_t{}},
    {"absent_strict", 0.0, 4.0, 6.0, strict, absent_t{}},
    {"strict_high_equal", 0.0, 4.0, 4.0, strict, absent_t{}},
    {"left_endpoint", 2.0, 9.0, 2.0, inclusive, finite_t{2.0}},
    {"right_endpoint", 0.0, 3.0, 2.0, strict, finite_t{3.0}},
    {"singleton_finite_satisfying", 2.0, 2.0, 2.0, inclusive, finite_t{2.0}},
    {"singleton_finite_nonsatisfying", 2.0, 2.0, 2.0, strict, absent_t{}},
    {"singleton_infinite", 5.0, 5.0, 4.0, strict, frontier_t{5.0}},
    {"entire_interval_infinite", 7.0, 9.0, 2.0, inclusive, frontier_t{7.0}},
    {"adjacent_finite", std::nextafter(scalar_t{1}, scalar_t{0}), 1.0, 2.0, inclusive, finite_t{1.0}},
    {"adjacent_frontier", std::nextafter(scalar_t{5}, scalar_t{0}), 5.0, 4.0, strict, frontier_t{5.0}},
    {"zero_target_inclusive", 0.0, 9.0, 0.0, inclusive, finite_t{0.0}},
    {"zero_target_strict", 0.0, 9.0, 0.0, strict, finite_t{1.0}},
};
INSTANTIATE_TEST_SUITE_P(
    boundaries, target_crossing_boundaries_test_t, ValuesIn(crossing_cases), test_name_generator_t<crossing_case_t>{});

TEST_F(target_crossing_integration_test_t, preserves_negative_zero_left_endpoint)
{
    auto const result = sut(scalar_t{-0.0}, scalar_t{0}, scalar_t{0}, inclusive, curve);
    ASSERT_TRUE(result);
    ASSERT_TRUE(std::holds_alternative<finite_t>(*result));
    EXPECT_TRUE(std::signbit(std::get<finite_t>(*result).input));
}

TEST_F(target_crossing_integration_test_t, collapses_zero_encodings_for_interior_crossing)
{
    struct zero_crossing_curve_t
    {
        auto try_evaluate(scalar_t input) const noexcept -> evaluation_t { return input < 0.0 ? 0.0 : 1.0; }
    };
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    auto const result = sut(-denorm, denorm, scalar_t{0}, strict, zero_crossing_curve_t{});
    ASSERT_TRUE(result);
    ASSERT_TRUE(std::holds_alternative<finite_t>(*result));
    EXPECT_EQ(std::get<finite_t>(*result).input, 0.0);
    EXPECT_FALSE(std::signbit(std::get<finite_t>(*result).input));
}

struct error_case_t
{
    char const* name;
    scalar_t input;
    error_t error;
};

struct target_crossing_error_test_t : target_crossing_integration_test_t, WithParamInterface<error_case_t>
{};

TEST_P(target_crossing_error_test_t, propagates_error_from_actual_search_probe)
{
    struct failing_curve_t
    {
        error_case_t param;
        auto try_evaluate(scalar_t input) const noexcept -> evaluation_t
        {
            if (input == param.input) return std::unexpected{param.error};
            return input < 0.0 ? 0.0 : 1.0;
        }
    };
    auto const& param = GetParam();
    EXPECT_EQ(
        sut(scalar_t{-1}, scalar_t{1}, scalar_t{0}, strict, failing_curve_t{param}), std::unexpected{param.error});
}

error_case_t const error_cases[] = {
    {"low", -1.0, error_t::negative_finite_result},
    {"high", 1.0, error_t::negative_infinity},
    {"interior", 0.0, error_t::nan},
};
INSTANTIATE_TEST_SUITE_P(
    probes, target_crossing_error_test_t, ValuesIn(error_cases), test_name_generator_t<error_case_t>{});

TEST_F(target_crossing_integration_test_t, accepts_left_crossing_without_probing_failing_right_endpoint)
{
    struct failing_right_curve_t
    {
        auto try_evaluate(scalar_t input) const noexcept -> evaluation_t
        {
            if (input == 9.0) return std::unexpected{error_t::nan};
            return 2.0;
        }
    };
    EXPECT_EQ(sut(scalar_t{1}, scalar_t{9}, scalar_t{2}, inclusive, failing_right_curve_t{}), outcome_t{finite_t{1.0}});
}

struct target_crossing_power_law_test_t : target_crossing_integration_test_t
{
    // the unit-power member with p = 1/2 doubles x exactly until overflow
    using power_law_t = model::curves::power_law_t;
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    curve_evaluator_t<evaluator_t> checked{evaluator_t{power_law_t::params_t<scalar_t>{0.5, 1.0}}};
    model::input_domain_t<scalar_t> domain = checked.input_domain();
};

TEST_F(target_crossing_power_law_test_t, finds_last_finite_output_across_admissible_domain)
{
    ASSERT_EQ(checked.try_evaluate(domain.last()), infinity);
    EXPECT_EQ(sut(domain.first(), domain.last(), max, inclusive, checked), outcome_t{finite_t{max / scalar_t{2}}});
}

TEST_F(target_crossing_power_law_test_t, classifies_first_overflow_when_no_larger_finite_output_exists)
{
    auto const frontier = std::nextafter(max / scalar_t{2}, max);
    ASSERT_EQ(checked.try_evaluate(frontier), infinity);
    EXPECT_EQ(sut(domain.first(), domain.last(), max, strict, checked), outcome_t{frontier_t{frontier}});
}

} // namespace
} // namespace crv::shaping
