// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual_estimator.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace {

struct residual_estimator_test_t : Test
{
    using real_t        = float_t;
    using approximant_t = int_t;

    static constexpr auto const approximant = approximant_t{123};
    static constexpr auto const left        = 0.0;
    static constexpr auto const right       = 10.0;

    using measured_error_t                             = measured_error_t<real_t>;
    static constexpr auto expected_max_error_magnitude = 10.0;

    static constexpr auto expected_positions           = std::array{0.0, 2.5, 5.0, 7.5, 10.0};
    static constexpr auto expected_quantized_positions = std::array{1.1, 2.2, 3.3, 4.4, 5.5};

    struct mock_target_function_t
    {
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
        virtual ~mock_target_function_t() = default;
    };
    StrictMock<mock_target_function_t> mock_target_function;

    struct target_function_t
    {
        mock_target_function_t* mock = nullptr;

        auto operator()(real_t position) const noexcept -> real_t { return mock->call(position); }
    };

    struct mock_approximant_evaluator_t
    {
        MOCK_METHOD(real_t, call, (approximant_t, real_t), (const, noexcept));
        virtual ~mock_approximant_evaluator_t() = default;
    };
    StrictMock<mock_approximant_evaluator_t> mock_approximant_evaluator;

    struct approximant_evaluator_t
    {
        mock_approximant_evaluator_t* mock = nullptr;

        auto operator()(approximant_t approximant, real_t position) const noexcept -> real_t
        {
            return mock->call(approximant, position);
        }
    };

    struct node_generator_t
    {
        using sample_locations_t = std::array<real_t, 5>;
        static constexpr sample_locations_t sample_locations{-1.0, -0.5, 0.0, 0.5, 1.0};
        auto operator()() const noexcept -> sample_locations_t const& { return sample_locations; }
    };

    struct mock_quantizer_t
    {
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
        virtual ~mock_quantizer_t() = default;
    };
    StrictMock<mock_quantizer_t> mock_quantizer;

    struct quantizer_t
    {
        mock_quantizer_t* mock = nullptr;

        auto operator()(real_t value) const noexcept -> real_t { return mock->call(value); }
    };

    using sut_t
        = residual_estimator_t<real_t, target_function_t, approximant_evaluator_t, node_generator_t, quantizer_t>;
    sut_t sut{
        .evaluate_target      = {&mock_target_function},
        .evaluate_approximant = {&mock_approximant_evaluator},
        .generate_nodes{},
        .quantize{&mock_quantizer},
    };

    // these must evaluate in ascending order to maximize quadrature cache hits
    InSequence seq;

    residual_estimator_test_t() {}

    auto expect_iteration(int_t position_index, real_t result = 0.0) -> void
    {
        auto const expected_position           = expected_positions[position_index];
        auto const expected_quantized_position = expected_quantized_positions[position_index];

        EXPECT_CALL(mock_quantizer, call(expected_position)).WillOnce(Return(expected_quantized_position));
        EXPECT_CALL(mock_target_function, call(expected_position)).WillOnce(Return(0.0));
        EXPECT_CALL(mock_approximant_evaluator, call(approximant, expected_quantized_position))
            .WillOnce(Return(result));
    }
};

TEST_F(residual_estimator_test_t, max_error_at_first_sample_location)
{
    expect_iteration(0, expected_max_error_magnitude);
    expect_iteration(1, expected_max_error_magnitude / 2);
    expect_iteration(2);
    expect_iteration(3);
    expect_iteration(4);

    auto const expected
        = measured_error_t{.position = expected_quantized_positions[0], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(approximant, left, right);

    EXPECT_EQ(actual, expected);
}

TEST_F(residual_estimator_test_t, max_error_at_last_sample_location)
{
    expect_iteration(0, expected_max_error_magnitude / 2);
    expect_iteration(1);
    expect_iteration(2);
    expect_iteration(3);
    expect_iteration(4, expected_max_error_magnitude);

    auto const expected
        = measured_error_t{.position = expected_quantized_positions[4], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(approximant, left, right);

    EXPECT_EQ(actual, expected);
}

TEST_F(residual_estimator_test_t, handles_negative_error_correctly)
{
    expect_iteration(0);
    expect_iteration(1, -expected_max_error_magnitude);
    expect_iteration(2);
    expect_iteration(3, expected_max_error_magnitude / 2);
    expect_iteration(4);

    auto const expected
        = measured_error_t{.position = expected_quantized_positions[1], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(approximant, left, right);

    EXPECT_EQ(actual, expected);
}

// test subbing exact boundaries and center
//
// sut_t subs literal {left, center, right} explicitly for sample locations at {-1, 0, +1}, respectively. This test
// deliberately generates a segment that would normally truncate left or right and makes sure the literal positions are
// still subbed.
TEST_F(residual_estimator_test_t, evaluates_exact_boundaries_despite_truncation)
{
    real_t expected_left  = 0.0;
    real_t expected_right = 0.0;

    // hunt for pair of values that do not correctly reconstruct left and right boundries after truncation
    for (auto i = 1; i < 10000; ++i)
    {
        auto const actual_left  = static_cast<real_t>(i) * 0.137;
        auto const actual_right = actual_left + 1.234;

        auto const center = (actual_right + actual_left) * 0.5;
        auto const radius = (actual_right - actual_left) * 0.5;

        // replicate sut calc
        auto const calculated_left  = center + radius * -1.0;
        auto const calculated_right = center + radius * 1.0;

        if (calculated_right != actual_right || calculated_left != actual_left)
        {
            expected_left  = actual_left;
            expected_right = actual_right;
            break;
        }
    }

    // ensure search didn't fail
    ASSERT_NE(expected_left, expected_right) << "failed to find truncation-inducing bounds";

    // cache expected values using same calcs as sut
    auto const expected_center     = (expected_right + expected_left) * 0.5;
    auto const expected_radius     = (expected_right - expected_left) * 0.5;
    auto const expected_position_1 = expected_center + expected_radius * node_generator_t::sample_locations[1];
    auto const expected_position_3 = expected_center + expected_radius * node_generator_t::sample_locations[3];

    // verify evaluations

    EXPECT_CALL(mock_quantizer, call(expected_left)).WillOnce(Return(expected_left));
    EXPECT_CALL(mock_target_function, call(expected_left)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_approximant_evaluator, call(approximant, expected_left)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_quantizer, call(expected_position_1)).WillOnce(Return(expected_position_1));
    EXPECT_CALL(mock_target_function, call(expected_position_1)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_approximant_evaluator, call(approximant, expected_position_1)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_quantizer, call(expected_center)).WillOnce(Return(expected_center));
    EXPECT_CALL(mock_target_function, call(expected_center)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_approximant_evaluator, call(approximant, expected_center)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_quantizer, call(expected_position_3)).WillOnce(Return(expected_position_3));
    EXPECT_CALL(mock_target_function, call(expected_position_3)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_approximant_evaluator, call(approximant, expected_position_3)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_quantizer, call(expected_right)).WillOnce(Return(expected_right));
    EXPECT_CALL(mock_target_function, call(expected_right)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_approximant_evaluator, call(approximant, expected_right)).WillOnce(Return(0.0));

    sut(approximant, expected_left, expected_right);
}

} // namespace
} // namespace crv
