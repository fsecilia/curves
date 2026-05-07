// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual_estimator.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

struct residual_estimator_test_t : Test
{
    using real_t = float_t;
    using approximant_t = int_t;

    static constexpr auto const approximant = approximant_t{123};
    static constexpr auto const left = 0.0;
    static constexpr auto const right = 10.0;

    static constexpr auto expected_max_error_magnitude = 11.1;

    static constexpr auto expected_positions = std::array{1.0, 2.5, 5.0, 7.5, 9.0};
    static constexpr auto expected_quantized_positions = std::array{1.1, 2.2, 3.3, 4.4, 5.5};
    static constexpr auto expected_targets = std::array{6.6, 7.7, 8.8, 9.9, 10.10};
    static constexpr auto expected_approximations = std::array{-6.6, -7.7, -8.8, -9.9, -10.10};
    static constexpr auto expected_magnitudes = std::array{101.0, 202.0, 303.0, 404.0, 505.0};

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
        static constexpr sample_locations_t sample_locations{0.1, 0.25, 0.5, 0.75, 0.9};
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

    struct mock_error_norm_t
    {
        MOCK_METHOD(real_t, call, (real_t, real_t), (const, noexcept));
        virtual ~mock_error_norm_t() = default;
    };
    StrictMock<mock_error_norm_t> mock_error_norm;

    struct error_norm_t
    {
        mock_error_norm_t* mock = nullptr;

        auto operator()(real_t target, real_t approximation) const noexcept -> real_t
        {
            return mock->call(target, approximation);
        }
    };

    struct mock_weight_function_t
    {
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
        virtual ~mock_weight_function_t() = default;
    };
    StrictMock<mock_weight_function_t> mock_weight_function;

    struct weight_function_t
    {
        mock_weight_function_t* mock = nullptr;

        auto operator()(real_t node) const noexcept -> real_t { return mock->call(node); }
    };

    using sut_t = residual_estimator_t<real_t, target_function_t, approximant_evaluator_t, node_generator_t,
        quantizer_t, error_norm_t, weight_function_t>;

    sut_t sut{
        .evaluate_target = {&mock_target_function},
        .evaluate_approximant = {&mock_approximant_evaluator},
        .generate_nodes{},
        .quantize{&mock_quantizer},
        .measure_error{&mock_error_norm},
        .apply_weight{&mock_weight_function},
    };

    // these must evaluate in ascending order to maximize quadrature cache hits
    InSequence seq;

    residual_estimator_test_t() {}

    auto expect_iteration(real_t position, real_t quantized_position, real_t target, real_t approximation,
        real_t magnitude, real_t result = 0.0) -> void
    {
        EXPECT_CALL(mock_quantizer, call(position)).WillOnce(Return(quantized_position));
        EXPECT_CALL(mock_target_function, call(position)).WillOnce(Return(target));
        EXPECT_CALL(mock_approximant_evaluator, call(approximant, quantized_position)).WillOnce(Return(approximation));
        EXPECT_CALL(mock_error_norm, call(target, approximation)).WillOnce(Return(magnitude));

        auto const required_weight = (magnitude == 0.0) ? 0.0 : abs(result) / magnitude;
        EXPECT_CALL(mock_weight_function, call(position)).WillOnce(Return(required_weight));
    }

    auto expect_iteration(int_t position_index, real_t result = 0.0) -> void
    {
        expect_iteration(expected_positions[position_index], expected_quantized_positions[position_index],
            expected_targets[position_index], expected_approximations[position_index],
            expected_magnitudes[position_index], result);
    }
};

TEST_F(residual_estimator_test_t, max_error_at_first_sample_location)
{
    expect_iteration(0, expected_max_error_magnitude);
    expect_iteration(1, expected_max_error_magnitude / 2);
    expect_iteration(2);
    expect_iteration(3);
    expect_iteration(4);

    auto const actual = sut(approximant, left, right);

    EXPECT_EQ(expected_max_error_magnitude, actual);
}

TEST_F(residual_estimator_test_t, max_error_at_last_sample_location)
{
    expect_iteration(0, expected_max_error_magnitude / 2);
    expect_iteration(1);
    expect_iteration(2);
    expect_iteration(3);
    expect_iteration(4, expected_max_error_magnitude);

    auto const actual = sut(approximant, left, right);

    EXPECT_EQ(expected_max_error_magnitude, actual);
}

TEST_F(residual_estimator_test_t, handles_negative_error_correctly)
{
    expect_iteration(0);
    expect_iteration(1, -expected_max_error_magnitude);
    expect_iteration(2);
    expect_iteration(3, expected_max_error_magnitude / 2);
    expect_iteration(4);

    auto const actual = sut(approximant, left, right);

    EXPECT_EQ(expected_max_error_magnitude, actual);
}

} // namespace
} // namespace crv::spline
