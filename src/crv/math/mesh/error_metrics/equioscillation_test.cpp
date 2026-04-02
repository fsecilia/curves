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

        for (auto node = 0u; node < count; ++node) { EXPECT_NEAR(expected[node], actual[node], 1e-10); }
    }
};

TEST_F(equioscillation_cached_nodes_test_t, count_5)
{
    // clang-format off
    static auto const expected = std::array{
        1.000000000000000000000000000000e+00,
        7.071067811865475727373109293694e-01,
        6.123233995736766035868820147292e-17,
        -7.071067811865474617150084668538e-01,
        -1.000000000000000000000000000000e+00,
    };
    // clang-format on

    compare(expected, node_cache_t<float_t, 5>::nodes);
}

TEST_F(equioscillation_cached_nodes_test_t, count_7)
{
    // clang-format off
    static auto const expected = std::array{
         1.000000000000000000000000000000e+00,
         8.660254037844387076106045242341e-01,
         5.000000000000001110223024625157e-01,
         6.123233995736766035868820147292e-17,
        -4.999999999999997779553950749687e-01,
        -8.660254037844384855659995992028e-01,
        -1.000000000000000000000000000000e+00,
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

    static constexpr auto       payload = payload_t{123};
    static constexpr auto const left    = 0.0;
    static constexpr auto const right   = 10.0;

    using measured_error_t                             = measured_error_t<real_t>;
    static constexpr auto expected_max_error_magnitude = 10.0;

    static constexpr auto expected_positions = std::array{2.5, 5.0, 7.5};

    struct node_cache_t
    {
        using nodes_t = std::array<real_t, 3>;
        static constexpr nodes_t nodes{-0.5, 0.0, 0.5};
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
        auto                   operator()(real_t position) const noexcept -> real_t { return mock->call(position); }
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

    equioscillation_error_metric_test_t()
    {
        EXPECT_CALL(mock_ideal_function, call(expected_positions[0])).WillOnce(Return(0.0));
        EXPECT_CALL(mock_ideal_function, call(expected_positions[1])).WillOnce(Return(0.0));
        EXPECT_CALL(mock_ideal_function, call(expected_positions[2])).WillOnce(Return(0.0));
    }
};

TEST_F(equioscillation_error_metric_test_t, max_error_at_first_node)
{
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[0])).WillOnce(Return(expected_max_error_magnitude));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[1]))
        .WillOnce(Return(expected_max_error_magnitude / 2));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[2])).WillOnce(Return(0.0));
    auto const expected
        = measured_error_t{.position = expected_positions[0], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(payload, left, right);

    EXPECT_EQ(actual, expected);
}

TEST_F(equioscillation_error_metric_test_t, max_error_at_last_node)
{
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[0]))
        .WillOnce(Return(expected_max_error_magnitude / 2));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[1])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[2])).WillOnce(Return(expected_max_error_magnitude));
    auto const expected
        = measured_error_t{.position = expected_positions[2], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(payload, left, right);

    EXPECT_EQ(actual, expected);
}

TEST_F(equioscillation_error_metric_test_t, handles_negative_error_correctly)
{
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[0])).WillOnce(Return(0.0));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[1])).WillOnce(Return(-expected_max_error_magnitude));
    EXPECT_CALL(mock_evaluator, call(payload, expected_positions[2]))
        .WillOnce(Return(expected_max_error_magnitude / 2));
    auto const expected
        = measured_error_t{.position = expected_positions[1], .magnitude = expected_max_error_magnitude};

    auto const actual = sut(payload, left, right);

    EXPECT_EQ(actual, expected);
}

} // namespace
} // namespace crv::equioscillation
