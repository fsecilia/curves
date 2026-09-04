// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "curve_evaluator.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/model/curves/concepts.hpp>
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

    struct evaluator_mock_t
    {
        virtual ~evaluator_mock_t() = default;
        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
        MOCK_METHOD(input_domain_t, input_domain, (), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
    };
    StrictMock<evaluator_mock_t> evaluator_mock;

    struct evaluator_t
    {
        using scalar_t = curve_evaluator_test_t::scalar_t;
        using jet_t = curve_evaluator_test_t::jet_t;

        evaluator_mock_t* mock;

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
    static_assert(is_curve<curve_evaluator_t, scalar_t>);

    curve_evaluator_t curve_evaluator{{&evaluator_mock}};
};

TEST_F(curve_evaluator_test_t, trusted_evaluation_returns_scalar)
{
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(5.0));
    EXPECT_EQ(curve_evaluator(3.0), 5.0);
}

TEST_F(curve_evaluator_test_t, trusted_evaluation_calls_wrapped_evaluator_once)
{
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(5.0));
    static_cast<void>(curve_evaluator(3.0));
}

TEST_F(curve_evaluator_test_t, jet_evaluation_forwards_to_wrapped_evaluator)
{
    auto const input = jet_t{3.0, 5.0};
    auto const output = jet_t{7.0, 11.0};
    EXPECT_CALL(evaluator_mock, jet(input)).WillOnce(Return(output));
    EXPECT_EQ(curve_evaluator(input), output);
}

TEST_F(curve_evaluator_test_t, input_domain_forwards_to_wrapped_evaluator)
{
    auto const domain = input_domain_t{3.0, 5.0};
    EXPECT_CALL(evaluator_mock, input_domain).WillOnce(Return(domain));
    EXPECT_EQ(curve_evaluator.input_domain(), domain);
}

TEST_F(curve_evaluator_test_t, critical_points_forward_to_wrapped_evaluator)
{
    auto const points = std::vector<scalar_t>{3.0, 5.0};
    EXPECT_CALL(evaluator_mock, critical_points).WillOnce(Return(points));
    EXPECT_EQ(curve_evaluator.critical_points(), points);
}

TEST_F(curve_evaluator_test_t, checked_finite_valid_evaluation_returns_scalar)
{
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(5.0));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), 5.0);
}

TEST_F(curve_evaluator_test_t, checked_finite_valid_evaluation_calls_wrapped_evaluator_once)
{
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(5.0));
    [[maybe_unused]] auto const result = curve_evaluator.try_evaluate(3.0);
}

TEST_F(curve_evaluator_test_t, checked_negative_finite_evaluation_returns_classified_error)
{
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(-5.0));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), std::unexpected{curve_evaluation_error_t::negative_finite_result});
}

TEST_F(curve_evaluator_test_t, checked_positive_infinity_returns_scalar)
{
    auto const infinity = std::numeric_limits<scalar_t>::infinity();
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(infinity));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), infinity);
}

TEST_F(curve_evaluator_test_t, checked_negative_infinity_returns_classified_error)
{
    auto const infinity = -std::numeric_limits<scalar_t>::infinity();
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(infinity));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), std::unexpected{curve_evaluation_error_t::negative_infinity});
}

TEST_F(curve_evaluator_test_t, checked_nan_returns_classified_error)
{
    auto const nan = std::numeric_limits<scalar_t>::quiet_NaN();
    EXPECT_CALL(evaluator_mock, scalar(3.0)).WillOnce(Return(nan));
    EXPECT_EQ(curve_evaluator.try_evaluate(3.0), std::unexpected{curve_evaluation_error_t::nan});
}

} // namespace
} // namespace crv::shaping
