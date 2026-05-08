// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual_estimator.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

struct residual_estimator_test_t : Test
{
    using real_t = float_t;
    using jet_t = jet_t<real_t>;
    using residual_t = residual_t<real_t>;

    struct target_function_sample_t
    {
        jet_t y;
        auto operator==(target_function_sample_t const&) const noexcept -> bool;
    };

    static constexpr auto left = 3.0;
    static constexpr auto right = 10.0;
    static constexpr auto interval_width = (right - left) * 0.5;
    static constexpr auto midpoint = (right + left) * 0.5;

    static constexpr auto expected_max_error_magnitude = 8.0;

    static constexpr auto expected_max_residual = residual_t{
        .metric_error = expected_max_error_magnitude,
        .weighted_error = 22.2,
        .scale = 33.3,
    };

    static constexpr auto node_count = 5;
    using nodes_t = std::array<real_t, node_count>;
    static constexpr auto expected_standard_nodes = nodes_t{0.1, 0.25, 0.5, 0.75, 0.9};
    static constexpr auto expected_quantized_nodes = nodes_t{1.1, 2.2, 3.3, 4.4, 5.5};
    static constexpr auto expected_targets = std::array<target_function_sample_t, node_count>{{
        {{6, 7}},
        {{8, 9}},
        {{10, 11}},
        {{12, 13}},
        {{expected_max_residual.scale, 15}},
    }};
    static constexpr auto expected_approximations
        = std::array<jet_t, node_count>{{{-1, -2}, {-3, -4}, {-5, -6}, {-7, -8}, {-9, -10}}};
    static constexpr auto expected_magnitudes = std::array{101.0, 202.0, 303.0, 404.0, 505.0};

    struct mock_function_sampler_t
    {
        virtual ~mock_function_sampler_t() = default;
        MOCK_METHOD(target_function_sample_t, call, (real_t), (const, noexcept));
    };
    StrictMock<mock_function_sampler_t> mock_function_sampler;

    struct function_sampler_t
    {
        mock_function_sampler_t* mock = nullptr;
        auto operator()(real_t position) const noexcept -> target_function_sample_t { return mock->call(position); }
    };
    function_sampler_t sample_target_function{&mock_function_sampler};

    struct mock_approximant_t
    {
        virtual ~mock_approximant_t() = default;
        MOCK_METHOD(jet_t, call, (real_t), (const, noexcept));
    };
    StrictMock<mock_approximant_t> mock_approximant;

    struct approximant_t
    {
        mock_approximant_t* mock = nullptr;
        auto operator()(real_t value) const noexcept -> jet_t { return mock->call(value); }
    };
    approximant_t approximant{&mock_approximant};

    struct node_generator_t
    {
        auto operator()() const noexcept -> nodes_t const& { return expected_standard_nodes; }
    };

    struct mock_quantizer_t
    {
        virtual ~mock_quantizer_t() = default;
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
    };
    StrictMock<mock_quantizer_t> mock_quantizer;

    struct quantizer_t
    {
        mock_quantizer_t* mock = nullptr;
        auto operator()(real_t value) const noexcept -> real_t { return mock->call(value); }
    };

    struct mock_error_norm_t
    {
        virtual ~mock_error_norm_t() = default;
        MOCK_METHOD(real_t, call, (jet_t, jet_t), (const, noexcept));
    };
    StrictMock<mock_error_norm_t> mock_error_norm;

    struct error_norm_t
    {
        mock_error_norm_t* mock = nullptr;
        auto operator()(jet_t target, jet_t approximation) const noexcept -> real_t
        {
            return mock->call(target, approximation);
        }
    };

    struct mock_weight_function_t
    {
        virtual ~mock_weight_function_t() = default;
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
    };
    StrictMock<mock_weight_function_t> mock_weight_function;

    struct weight_function_t
    {
        mock_weight_function_t* mock = nullptr;

        auto operator()(real_t node) const noexcept -> real_t { return mock->call(node); }
    };

    using sut_t = residual_estimator_t<real_t, node_generator_t, quantizer_t, error_norm_t, weight_function_t>;

    sut_t sut{
        .generate_nodes{},
        .quantize{&mock_quantizer},
        .measure_error{&mock_error_norm},
        .apply_weight{&mock_weight_function},
    };

    // these must evaluate in ascending order to maximize quadrature cache hits
    InSequence seq;

    auto expect_iteration(real_t standard_node, real_t quantized_node, target_function_sample_t target,
        jet_t approximation, real_t magnitude) -> void
    {
        auto const domain_node = left + standard_node * interval_width;
        EXPECT_CALL(mock_quantizer, call(domain_node)).WillOnce(Return(quantized_node));
        EXPECT_CALL(mock_approximant, call(quantized_node)).WillOnce(Return(approximation));
        EXPECT_CALL(mock_function_sampler, call(domain_node)).WillOnce(Return(target));
        EXPECT_CALL(mock_error_norm, call(target.y, approximation)).WillOnce(Return(magnitude));
    }

    auto expect_weight()
    {
        auto const required_metric_error = abs(expected_max_residual.weighted_error) / expected_max_error_magnitude;
        EXPECT_CALL(mock_weight_function, call(midpoint)).WillOnce(Return(required_metric_error));
    }

    auto expect_iteration(int_t node_index, real_t magnitude = 0.0) -> void
    {
        expect_iteration(expected_standard_nodes[node_index], expected_quantized_nodes[node_index],
            expected_targets[node_index], expected_approximations[node_index], magnitude);
    }
};

TEST_F(residual_estimator_test_t, max_error_at_first_sample_location)
{
    expect_iteration(0, expected_max_error_magnitude);
    expect_iteration(1, expected_max_error_magnitude / 2);
    expect_iteration(2);
    expect_iteration(3);
    expect_iteration(4);
    expect_weight();

    auto const actual = sut(sample_target_function, approximant, left, right);

    EXPECT_EQ(expected_max_residual, actual);
}

TEST_F(residual_estimator_test_t, max_error_at_last_sample_location)
{
    expect_iteration(0, expected_max_error_magnitude / 2);
    expect_iteration(1);
    expect_iteration(2);
    expect_iteration(3);
    expect_iteration(4, expected_max_error_magnitude);
    expect_weight();

    auto const actual = sut(sample_target_function, approximant, left, right);

    EXPECT_EQ(expected_max_residual, actual);
}

TEST_F(residual_estimator_test_t, handles_negative_error_correctly)
{
    expect_iteration(0);
    expect_iteration(1, -expected_max_error_magnitude);
    expect_iteration(2);
    expect_iteration(3, expected_max_error_magnitude / 2);
    expect_iteration(4);
    expect_weight();

    auto const actual = sut(sample_target_function, approximant, left, right);

    EXPECT_EQ(expected_max_residual, actual);
}

} // namespace
} // namespace crv::spline
