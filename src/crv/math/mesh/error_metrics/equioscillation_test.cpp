// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "equioscillation.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::equioscillation {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// Node Cache
// --------------------------------------------------------------------------------------------------------------------

struct equioscillation_cached_nodes_test_t : Test
{
    auto compare(auto const& expected, auto const& actual) const noexcept -> void
    {
        constexpr auto count = std::size(expected);
        static_assert(count == std::size(actual));

        for (auto node = 0u; node < count; ++node) { EXPECT_DOUBLE_EQ(expected[node], actual[node]); }
    }
};

TEST_F(equioscillation_cached_nodes_test_t, count_5)
{
    // clang-format off
    static auto const expected = std::array{
        -1.000000000000000e+00,
        -7.071067811865475e-01,
         0.000000000000000e+00,
         7.071067811865474e-01,
         1.000000000000000e+00,
    };
    // clang-format on

    compare(expected, node_cache_t<float_t, 5>::nodes);
}

TEST_F(equioscillation_cached_nodes_test_t, count_7)
{
    // clang-format off
    static auto const expected = std::array{
        -1.000000000000000e+00,
        -8.660254037844387e-01,
        -5.000000000000001e-01,
         0.000000000000000e+00,
         4.999999999999997e-01,
         8.660254037844384e-01,
         1.000000000000000e+00,
    };
    // clang-format on

    compare(expected, node_cache_t<float_t, 7>::nodes);
}

// --------------------------------------------------------------------------------------------------------------------
// Error Metric
// --------------------------------------------------------------------------------------------------------------------

struct equioscillation_error_metric_test_t : Test
{
    using real_t    = float_t;
    using payload_t = int_t;

    static constexpr auto const payload = payload_t{123};
    static constexpr auto const left    = 0.0;
    static constexpr auto const right   = 10.0;

    using measured_error_t                             = measured_error_t<real_t>;
    static constexpr auto expected_max_error_magnitude = 10.0;

    static constexpr auto expected_positions = std::array{0.0, 2.5, 5.0, 7.5, 10.0};

    struct node_cache_t
    {
        using nodes_t = std::array<real_t, 5>;
        static constexpr nodes_t nodes{-1.0, -0.5, 0.0, 0.5, 1.0};
    };

    struct mock_ideal_function_t
    {
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
        virtual ~mock_ideal_function_t() = default;
    };
    StrictMock<mock_ideal_function_t> mock_ideal_function;

    struct ideal_function_t
    {
        mock_ideal_function_t* mock = nullptr;

        auto operator()(real_t position) const noexcept -> real_t { return mock->call(position); }
    };

    struct mock_evaluator_t
    {
        MOCK_METHOD(real_t, call, (payload_t, real_t), (const, noexcept));
        virtual ~mock_evaluator_t() = default;
    };
    StrictMock<mock_evaluator_t> mock_evaluator;

    struct evaluator_t
    {
        mock_evaluator_t* mock = nullptr;

        auto operator()(payload_t payload, real_t position) const noexcept -> real_t
        {
            return mock->call(payload, position);
        }
    };

    using sut_t = error_metric_t<real_t, node_cache_t, ideal_function_t, evaluator_t>;
    sut_t sut{.ideal_function = {&mock_ideal_function}, .evaluator = {&mock_evaluator}};

    // these must evaluate in ascending order to maximize quadrature cache hits
    InSequence seq;

    equioscillation_error_metric_test_t() {}
};

TEST_F(equioscillation_error_metric_test_t, max_error_at_first_node)
{
    EXPECT_CALL(mock_ideal_function, call(expected_positions[0])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[0])).WillOnce(Return(expected_max_error_magnitude));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[1])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[1]))
        .WillOnce(Return(expected_max_error_magnitude / 2));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[2])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[2])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_positions[3])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[3])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_positions[4])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[4])).WillOnce(Return(0.0));

    auto const expected
        = measured_error_t{.position = expected_positions[0], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(payload, left, right);

    EXPECT_EQ(actual, expected);
}

TEST_F(equioscillation_error_metric_test_t, max_error_at_last_node)
{
    EXPECT_CALL(mock_ideal_function, call(expected_positions[0])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[0]))
        .WillOnce(Return(expected_max_error_magnitude / 2));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[1])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[1])).WillOnce(Return(0.0));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[2])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[2])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_positions[3])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[3])).WillOnce(Return(0.0));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[4])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[4])).WillOnce(Return(expected_max_error_magnitude));

    auto const expected
        = measured_error_t{.position = expected_positions[4], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(payload, left, right);

    EXPECT_EQ(actual, expected);
}

TEST_F(equioscillation_error_metric_test_t, handles_negative_error_correctly)
{
    EXPECT_CALL(mock_ideal_function, call(expected_positions[0])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[0])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_positions[1])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[1])).WillOnce(Return(-expected_max_error_magnitude));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[2])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[2]))
        .WillOnce(Return(expected_max_error_magnitude / 2));

    EXPECT_CALL(mock_ideal_function, call(expected_positions[3])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[3])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_positions[4])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[4])).WillOnce(Return(0.0));

    auto const expected
        = measured_error_t{.position = expected_positions[1], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(payload, left, right);

    EXPECT_EQ(actual, expected);
}

// test subbing exact boundaries and center
//
// sut_t subs literal {left, center, right} explicitly for nodes at {-1, 0, +1}, respectively. This test deliberately
// generates a segment that would normally truncate left or right and makes sure the literal positions are still subbed.
TEST_F(equioscillation_error_metric_test_t, evaluates_exact_boundaries_despite_truncation)
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
    auto const expected_position_1 = expected_center + expected_radius * node_cache_t::nodes[1];
    auto const expected_position_3 = expected_center + expected_radius * node_cache_t::nodes[3];

    // verify evaluations

    EXPECT_CALL(mock_ideal_function, call(expected_left)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_left)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_position_1)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_position_1)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_center)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_center)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_position_3)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_position_3)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_ideal_function, call(expected_right)).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_right)).WillOnce(Return(0.0));

    sut(payload, expected_left, expected_right);
}

} // namespace
} // namespace crv::equioscillation
