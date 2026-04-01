// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/mesh/error_metrics/chebyshev.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::chebyshev {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// Node Cache
// --------------------------------------------------------------------------------------------------------------------

struct chebyshev_cached_nodes_test_t : Test
{
    auto compare(auto const& expected, auto const& actual) const noexcept -> void
    {
        constexpr auto count = std::size(expected);
        static_assert(count == std::size(actual));

        for (auto node = 0u; node < count; ++node) { EXPECT_NEAR(expected[node], actual[node], 1e-10); }
    }
};

TEST_F(chebyshev_cached_nodes_test_t, count_5)
{
    static auto const expected = std::array{
        9.510565162951535311819384332921e-01,  5.877852522924731371034567928291e-01,
        6.123233995736766035868820147292e-17,  -5.877852522924730260811543303134e-01,
        -9.510565162951535311819384332921e-01,
    };

    compare(expected, node_cache_t<float_t, 5>::nodes);
}

TEST_F(chebyshev_cached_nodes_test_t, count_7)
{
    static auto const expected = std::array{
        9.749279121818236193419693336182e-01,  7.818314824680298036341241640912e-01,
        4.338837391175581759128476733167e-01,  6.123233995736766035868820147292e-17,
        -4.338837391175580648905452108011e-01, -7.818314824680298036341241640912e-01,
        -9.749279121818236193419693336182e-01,
    };

    compare(expected, node_cache_t<float_t, 7>::nodes);
}

// --------------------------------------------------------------------------------------------------------------------
// Error Metric
// --------------------------------------------------------------------------------------------------------------------

struct chebyshev_error_metric_test_t : Test
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

    chebyshev_error_metric_test_t()
    {
        EXPECT_CALL(mock_ideal_function, call(expected_positions[0])).WillOnce(Return(0.0));
        EXPECT_CALL(mock_ideal_function, call(expected_positions[1])).WillOnce(Return(0.0));
        EXPECT_CALL(mock_ideal_function, call(expected_positions[2])).WillOnce(Return(0.0));
    }
};

TEST_F(chebyshev_error_metric_test_t, max_error_at_first_node)
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

TEST_F(chebyshev_error_metric_test_t, max_error_at_last_node)
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

TEST_F(chebyshev_error_metric_test_t, handles_negative_error_correctly)
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
} // namespace crv::chebyshev
