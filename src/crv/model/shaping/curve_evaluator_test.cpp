// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "curve_evaluator.hpp"
#include <crv/model/curves/concepts.hpp>
#include <crv/model/curves/power_law.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <limits>
#include <vector>

namespace crv::shaping {
namespace {

struct curve_evaluator_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;
    using input_domain_t = model::input_domain_t<scalar_t>;

    struct mock_evaluator_t
    {
        virtual ~mock_evaluator_t() = default;
        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
        MOCK_METHOD(input_domain_t, input_domain, (), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
    };
    StrictMock<mock_evaluator_t> mock_evaluator;

    struct evaluator_t
    {
        using scalar_t = curve_evaluator_test_t::scalar_t;
        using jet_t = curve_evaluator_test_t::jet_t;

        mock_evaluator_t* mock;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
        [[nodiscard]] auto input_domain() const noexcept -> input_domain_t { return mock->input_domain(); }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return mock->critical_points(); }
    };

    using curve_evaluator_t = shaping::curve_evaluator_t<evaluator_t>;

    static_assert(std::same_as<decltype(std::declval<curve_evaluator_t const&>()(scalar_t{})), scalar_t>);
    static_assert(std::same_as<decltype(std::declval<curve_evaluator_t const&>().try_evaluate(scalar_t{})),
        curve_evaluator_t::result_t>);
    static_assert(std::same_as<typename curve_evaluator_t::result_t::value_type, scalar_t>);
    static_assert(std::same_as<typename curve_evaluator_t::result_t::error_type, curve_evaluation_error_t>);
    static_assert(std::same_as<decltype(std::declval<curve_evaluator_t const&>().try_evaluate(jet_t{})),
        std::expected<jet_t, curve_evaluation_error_t>>);
    static_assert(std::same_as<curve_evaluator_t::jet_result_t, std::expected<jet_t, curve_evaluation_error_t>>);
    static_assert(noexcept(std::declval<curve_evaluator_t const&>().try_evaluate(jet_t{})));
    static_assert(is_curve<curve_evaluator_t, scalar_t>);

    static constexpr auto inf_value = std::numeric_limits<scalar_t>::infinity();
    static constexpr auto nan_value = std::numeric_limits<scalar_t>::quiet_NaN();
    static constexpr auto jet_input = jet_t{3.0, 5.0};

    curve_evaluator_t curve_evaluator{{&mock_evaluator}};
};

TEST_F(curve_evaluator_test_t, trusted_evaluation_returns_scalar)
{
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(5.0));
    EXPECT_EQ(curve_evaluator(3.0), 5.0);
}

TEST_F(curve_evaluator_test_t, trusted_evaluation_calls_wrapped_evaluator_once)
{
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(5.0));
    static_cast<void>(curve_evaluator(3.0));
}

TEST_F(curve_evaluator_test_t, jet_evaluation_forwards_to_wrapped_evaluator)
{
    auto const input = jet_t{3.0, 5.0};
    auto const output = jet_t{7.0, 11.0};
    EXPECT_CALL(mock_evaluator, jet(input)).WillOnce(Return(output));
    EXPECT_EQ(curve_evaluator(input), output);
}

TEST_F(curve_evaluator_test_t, input_domain_forwards_to_wrapped_evaluator)
{
    auto const domain = input_domain_t{3.0, 5.0};
    EXPECT_CALL(mock_evaluator, input_domain).WillOnce(Return(domain));
    EXPECT_EQ(curve_evaluator.input_domain(), domain);
}

TEST_F(curve_evaluator_test_t, critical_points_forward_to_wrapped_evaluator)
{
    auto const points = std::vector<scalar_t>{3.0, 5.0};
    EXPECT_CALL(mock_evaluator, critical_points).WillOnce(Return(points));
    EXPECT_EQ(curve_evaluator.critical_points(), points);
}

TEST_F(curve_evaluator_test_t, checked_finite_valid_evaluation_returns_scalar)
{
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(5.0));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), 5.0);
}

TEST_F(curve_evaluator_test_t, checked_finite_valid_evaluation_calls_wrapped_evaluator_once)
{
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(5.0));
    [[maybe_unused]] auto const result = curve_evaluator.try_evaluate(3.0);
}

TEST_F(curve_evaluator_test_t, checked_negative_finite_evaluation_returns_classified_error)
{
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(-5.0));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), std::unexpected{curve_evaluation_error_t::negative_finite_result});
}

TEST_F(curve_evaluator_test_t, checked_positive_infinity_returns_scalar)
{
    auto const infinity = std::numeric_limits<scalar_t>::infinity();
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(infinity));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), infinity);
}

TEST_F(curve_evaluator_test_t, checked_negative_infinity_returns_classified_error)
{
    auto const infinity = -std::numeric_limits<scalar_t>::infinity();
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(infinity));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), std::unexpected{curve_evaluation_error_t::negative_infinity});
}

TEST_F(curve_evaluator_test_t, checked_nan_returns_classified_error)
{
    auto const nan = std::numeric_limits<scalar_t>::quiet_NaN();
    EXPECT_CALL(mock_evaluator, scalar(3.0)).WillOnce(Return(nan));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), std::unexpected{curve_evaluation_error_t::nan});
}

TEST_F(curve_evaluator_test_t, checked_jet_returns_positive_finite_tangent)
{
    auto const output = jet_t{7.0, 11.0};
    EXPECT_CALL(mock_evaluator, jet(jet_input)).WillOnce(Return(output));
    EXPECT_EQ(curve_evaluator.try_evaluate(jet_input), output);
}

TEST_F(curve_evaluator_test_t, checked_jet_returns_negative_finite_tangent)
{
    auto const output = jet_t{7.0, -11.0};
    EXPECT_CALL(mock_evaluator, jet(jet_input)).WillOnce(Return(output));
    EXPECT_EQ(curve_evaluator.try_evaluate(jet_input), output);
}

TEST_F(curve_evaluator_test_t, checked_jet_returns_zero_primal_and_tangent)
{
    auto const input = jet_t{0.0, 0.0};
    auto const output = jet_t{0.0, 0.0};
    EXPECT_CALL(mock_evaluator, jet(input)).WillOnce(Return(output));
    EXPECT_EQ(curve_evaluator.try_evaluate(input), output);
}

struct jet_failure_t
{
    char const* name;
    curve_evaluator_test_t::jet_t output;
    curve_evaluation_error_t error;
};

struct checked_jet_failure_test_t : curve_evaluator_test_t, WithParamInterface<jet_failure_t>
{};

TEST_P(checked_jet_failure_test_t, classifies_output)
{
    auto const& test = GetParam();
    EXPECT_CALL(mock_evaluator, jet(jet_input)).WillOnce(Return(test.output));
    EXPECT_EQ(curve_evaluator.try_evaluate(jet_input), std::unexpected{test.error});
}

INSTANTIATE_TEST_SUITE_P(output, checked_jet_failure_test_t,
    Values(jet_failure_t{"negative_finite_primal", {-7.0, 11.0}, curve_evaluation_error_t::negative_finite_result},
        jet_failure_t{"negative_infinite_primal", {-curve_evaluator_test_t::inf_value, 11.0},
            curve_evaluation_error_t::negative_infinity},
        jet_failure_t{"nan_primal", {curve_evaluator_test_t::nan_value, 11.0}, curve_evaluation_error_t::nan},
        jet_failure_t{"positive_infinite_primal", {curve_evaluator_test_t::inf_value, 11.0},
            curve_evaluation_error_t::positive_infinite_primal},
        jet_failure_t{"positive_infinite_tangent", {7.0, curve_evaluator_test_t::inf_value},
            curve_evaluation_error_t::nonfinite_derivative},
        jet_failure_t{"negative_infinite_tangent", {7.0, -curve_evaluator_test_t::inf_value},
            curve_evaluation_error_t::nonfinite_derivative},
        jet_failure_t{
            "nan_tangent", {7.0, curve_evaluator_test_t::nan_value}, curve_evaluation_error_t::nonfinite_derivative}),
    test_name_generator_t<jet_failure_t>{});

INSTANTIATE_TEST_SUITE_P(primal_precedence, checked_jet_failure_test_t,
    Values(jet_failure_t{"negative_finite_primal", {-7.0, curve_evaluator_test_t::nan_value},
               curve_evaluation_error_t::negative_finite_result},
        jet_failure_t{"negative_infinite_primal",
            {-curve_evaluator_test_t::inf_value, curve_evaluator_test_t::inf_value},
            curve_evaluation_error_t::negative_infinity},
        jet_failure_t{"nan_primal", {curve_evaluator_test_t::nan_value, -curve_evaluator_test_t::inf_value},
            curve_evaluation_error_t::nan},
        jet_failure_t{"positive_infinite_primal",
            {curve_evaluator_test_t::inf_value, curve_evaluator_test_t::nan_value},
            curve_evaluation_error_t::positive_infinite_primal}),
    test_name_generator_t<jet_failure_t>{});

struct checked_jet_input_failure_test_t : curve_evaluator_test_t, WithParamInterface<curve_evaluator_test_t::scalar_t>
{};

TEST_P(checked_jet_input_failure_test_t, rejects_nonfinite_tangent_without_evaluating)
{
    EXPECT_EQ(curve_evaluator.try_evaluate(jet_t{3.0, GetParam()}),
        std::unexpected{curve_evaluation_error_t::nonfinite_input_tangent});
}

INSTANTIATE_TEST_SUITE_P(input, checked_jet_input_failure_test_t,
    Values(curve_evaluator_test_t::inf_value, -curve_evaluator_test_t::inf_value, curve_evaluator_test_t::nan_value));

struct checked_power_law_integration_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;
    using power_law_t = model::curves::power_law_t;
    using evaluator_t = power_law_t::evaluator_t<scalar_t>;
    using params_t = power_law_t::params_t<scalar_t>;

    curve_evaluator_t<evaluator_t> curve{evaluator_t{params_t{.unit_speed = 1.0, .power = 0.5}}};
};

TEST_F(checked_power_law_integration_test_t, scalar_origin_is_valid)
{
    EXPECT_EQ(curve.try_evaluate(scalar_t{0}), scalar_t{0});
}

struct singular_power_law_jet_test_t : checked_power_law_integration_test_t,
                                       WithParamInterface<checked_power_law_integration_test_t::scalar_t>
{};

TEST_P(singular_power_law_jet_test_t, origin_has_nonfinite_derivative)
{
    EXPECT_EQ(
        curve.try_evaluate(jet_t{0.0, GetParam()}), std::unexpected{curve_evaluation_error_t::nonfinite_derivative});
}

INSTANTIATE_TEST_SUITE_P(origin, singular_power_law_jet_test_t, Values(1.0, -1.0, 0.0));

TEST_F(checked_power_law_integration_test_t, away_from_origin_returns_finite_jet)
{
    EXPECT_EQ(curve.try_evaluate(jet_t{1.0, 1.0}), (jet_t{1.0, 0.5}));
}

TEST_F(checked_power_law_integration_test_t, overflowing_scalar_remains_valid_evidence)
{
    auto const overflowing_curve = curve_evaluator_t{evaluator_t{params_t{.unit_speed = 1.0, .power = 1024.0}}};
    EXPECT_EQ(overflowing_curve.try_evaluate(scalar_t{4}), std::numeric_limits<scalar_t>::infinity());
}

TEST_F(checked_power_law_integration_test_t, overflowing_jet_is_rejected)
{
    auto const overflowing_curve = curve_evaluator_t{evaluator_t{params_t{.unit_speed = 1.0, .power = 1024.0}}};
    EXPECT_EQ(overflowing_curve.try_evaluate(jet_t{4.0, 1.0}),
        std::unexpected{curve_evaluation_error_t::positive_infinite_primal});
}

} // namespace
} // namespace crv::shaping
