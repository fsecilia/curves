// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_bound.hpp"
#include <crv/model/curves/power_law.hpp>
#include <crv/model/shaping/curve_evaluator.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace crv::model::curves {
namespace {

struct curve_output_bound_integration_test_t : Test
{
    using scalar_t = float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;

    struct plateau_evaluator_t
    {
        using scalar_t = curve_output_bound_integration_test_t::scalar_t;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t
        {
            if (input < scalar_t{1}) return scalar_t{0};
            if (input <= scalar_t{2}) return scalar_t{1};
            return scalar_t{2};
        }

        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {scalar_t{0}, scalar_t{3}};
        }

        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {scalar_t{1}, scalar_t{2}}; }
    };

    struct overflowing_identity_evaluator_t
    {
        using scalar_t = curve_output_bound_integration_test_t::scalar_t;

        scalar_t frontier;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t
        {
            if (input >= frontier) return std::numeric_limits<scalar_t>::infinity();
            return input;
        }

        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {scalar_t{0}, scalar_t{4}};
        }

        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {frontier}; }
    };

    using plateau_curve_t = shaping::curve_evaluator_t<plateau_evaluator_t>;
    using overflowing_identity_curve_t = shaping::curve_evaluator_t<overflowing_identity_evaluator_t>;

    static_assert(is_curve<plateau_curve_t, scalar_t>);
    static_assert(is_curve<overflowing_identity_curve_t, scalar_t>);

    curve_output_lower_bound_t lower_bound{};
    curve_output_upper_bound_t upper_bound{};
    plateau_curve_t plateau_curve{plateau_evaluator_t{}};
};

TEST_F(curve_output_bound_integration_test_t, lower_bound_returns_start_of_equality_plateau)
{
    auto const expected = curve_output_bound_result_t<scalar_t>{std::optional<scalar_t>{1}};
    EXPECT_EQ(lower_bound(plateau_curve, input_domain_t{0, 3}, scalar_t{1}), expected);
}

TEST_F(curve_output_bound_integration_test_t, upper_bound_returns_first_representable_input_after_equality_plateau)
{
    auto const expected = std::nextafter(scalar_t{2}, std::numeric_limits<scalar_t>::infinity());
    auto const expected_result = curve_output_bound_result_t<scalar_t>{std::optional<scalar_t>{expected}};
    EXPECT_EQ(upper_bound(plateau_curve, input_domain_t{0, 3}, scalar_t{1}), expected_result);
}

TEST_F(curve_output_bound_integration_test_t, bound_already_satisfied_at_low_returns_low)
{
    auto const expected = curve_output_bound_result_t<scalar_t>{std::optional<scalar_t>{1}};
    EXPECT_EQ(lower_bound(plateau_curve, input_domain_t{1, 3}, scalar_t{1}), expected);
}

TEST_F(curve_output_bound_integration_test_t, finite_high_endpoint_below_target_returns_absence)
{
    auto const expected = curve_output_bound_result_t<scalar_t>{std::optional<scalar_t>{}};
    EXPECT_EQ(lower_bound(plateau_curve, input_domain_t{0, 3}, scalar_t{3}), expected);
}

TEST_F(curve_output_bound_integration_test_t, finite_crossing_before_frontier_returns_crossing)
{
    auto const curve = overflowing_identity_curve_t{overflowing_identity_evaluator_t{2}};
    auto const expected = curve_output_bound_result_t<scalar_t>{std::optional<scalar_t>{1}};
    EXPECT_EQ(lower_bound(curve, input_domain_t{0, 4}, scalar_t{1}), expected);
}

TEST_F(curve_output_bound_integration_test_t, frontier_before_finite_crossing_returns_bound_resolution_error)
{
    auto const curve = overflowing_identity_curve_t{overflowing_identity_evaluator_t{2}};
    auto const expected = curve_output_bound_result_t<scalar_t>{std::unexpected{curve_output_bound_errors_t{
        std::in_place_type<curve_output_bound_error_t>, curve_output_bound_error_t::frontier_preceded_bound}}};
    EXPECT_EQ(lower_bound(curve, input_domain_t{0, 4}, scalar_t{3}), expected);
}

TEST_F(curve_output_bound_integration_test_t, frontier_at_search_low_returns_bound_resolution_error)
{
    auto const curve = overflowing_identity_curve_t{overflowing_identity_evaluator_t{2}};
    auto const expected = curve_output_bound_result_t<scalar_t>{std::unexpected{curve_output_bound_errors_t{
        std::in_place_type<curve_output_bound_error_t>, curve_output_bound_error_t::frontier_preceded_bound}}};
    EXPECT_EQ(lower_bound(curve, input_domain_t{2, 4}, scalar_t{3}), expected);
}

TEST_F(curve_output_bound_integration_test_t, overflowing_power_law_finds_finite_crossing_before_frontier)
{
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    using curve_t = shaping::curve_evaluator_t<evaluator_t>;

    auto const curve = curve_t{evaluator_t{power_law_t::params_t<scalar_t>{1, 256}}};
    auto const expected = curve_output_bound_result_t<scalar_t>{std::optional<scalar_t>{1}};
    EXPECT_EQ(lower_bound(curve, curve.input_domain(), scalar_t{1}), expected);
}

} // namespace
} // namespace crv::model::curves
