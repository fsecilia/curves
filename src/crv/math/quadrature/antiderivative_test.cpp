// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "antiderivative.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature::generic {
namespace {

// ====================================================================================================================
// antiderivative_t
// ====================================================================================================================

struct quadrature_antiderivative_test_t : Test
{
    using real_t = float_t;
    using jet_t  = jet_t<real_t>;

    struct mock_integral_t
    {
        MOCK_METHOD(real_t, integrate, (real_t left, real_t right), (const, noexcept));
        MOCK_METHOD(real_t, evaluate_integrand, (real_t location), (const, noexcept));
        virtual ~mock_integral_t() = default;
    };
    StrictMock<mock_integral_t> mock_integral;

    struct integral_t
    {
        using real_t = float_t;

        mock_integral_t* mock = nullptr;

        auto integrate(real_t left, real_t right) const noexcept -> real_t { return mock->integrate(left, right); }
        auto evaluate_integrand(real_t location) const noexcept -> real_t { return mock->evaluate_integrand(location); }
    };

    using sut_t = antiderivative_t<integral_t>;

    static constexpr auto expected_residual   = 0.3174;
    static constexpr auto expected_derivative = 1.7213;

    auto expect_location(real_t location, real_t expected_left, real_t) -> void
    {
        EXPECT_CALL(mock_integral, integrate(expected_left, location)).WillOnce(Return(expected_residual));
        EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));
    }
};

// --------------------------------------------------------------------------------------------------------------------
// small cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_antiderivative_test_small_cache_t : quadrature_antiderivative_test_t
{
    sut_t sut{
        integral_t{&mock_integral},
        {
            {0.0, 0.0},
            {1.0, 2.5},
            {2.0, 5.0},
            {3.0, 8.5},
        },
    };
};

TEST_F(quadrature_antiderivative_test_small_cache_t, segment_count)
{
    EXPECT_EQ(3, sut.segment_count());
}

// test absolute left edge of the domain
TEST_F(quadrature_antiderivative_test_small_cache_t, domain_min)
{
    auto const location               = 0.0;
    auto const expected_left_position = 0.0;
    auto const expected_left_sum      = 0.0;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// tests point inside first segment, 0.0 <= x < 1.0
TEST_F(quadrature_antiderivative_test_small_cache_t, first_segment)
{
    auto const location               = 0.5;
    auto const expected_left_position = 0.0;
    auto const expected_left_sum      = 0.0;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// tests point exactly on first boundary
TEST_F(quadrature_antiderivative_test_small_cache_t, first_boundary)
{
    auto const location               = 1.0;
    auto const expected_left_position = 1.0;
    auto const expected_left_sum      = 2.5;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// test point inside a middle interval, 1.0 < x < 2.0
TEST_F(quadrature_antiderivative_test_small_cache_t, middle_interval)
{
    auto const location               = 1.5;
    auto const expected_left_position = 1.0;
    auto const expected_left_sum      = 2.5;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// test absolute right edge of the domain
TEST_F(quadrature_antiderivative_test_small_cache_t, domain_max)
{
    auto const location               = 3.0;
    auto const expected_left_position = 3.0;
    auto const expected_left_sum      = 8.5;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS

// test queries below the valid domain bounds
TEST_F(quadrature_antiderivative_test_small_cache_t, query_below_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut(-0.01), "domain error");
}

// test queries strictly above the valid domain bounds
TEST_F(quadrature_antiderivative_test_small_cache_t, query_above_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut(3.01), "domain error");
}

// test NaN injection
TEST_F(quadrature_antiderivative_test_small_cache_t, query_nan_aborts)
{
    EXPECT_DEBUG_DEATH(sut(std::numeric_limits<real_t>::quiet_NaN()), "domain error");
}

#endif

// --------------------------------------------------------------------------------------------------------------------
// minimal cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_antiderivative_test_minimal_cache_t : quadrature_antiderivative_test_t
{
    sut_t sut{
        integral_t{&mock_integral},
        {
            {0.0, 0.0},
            {1.5, 3.0},
        },
    };
};

TEST_F(quadrature_antiderivative_test_minimal_cache_t, segment_count)
{
    EXPECT_EQ(1, sut.segment_count());
}

// test absolute left edge of the domain
TEST_F(quadrature_antiderivative_test_minimal_cache_t, domain_min)
{
    auto const location               = 0.0;
    auto const expected_left_position = 0.0;
    auto const expected_left_sum      = 0.0;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// test interior
TEST_F(quadrature_antiderivative_test_minimal_cache_t, interior)
{
    auto const location               = 0.75;
    auto const expected_left_position = 0.0;
    auto const expected_left_sum      = 0.0;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// test absolute right edge of the domain
TEST_F(quadrature_antiderivative_test_minimal_cache_t, domain_max)
{
    auto const location               = 1.5;
    auto const expected_left_position = 1.5;
    auto const expected_left_sum      = 3.0;
    EXPECT_CALL(mock_integral, integrate(expected_left_position, location)).WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integral, evaluate_integrand(location)).WillOnce(Return(expected_derivative));

    EXPECT_EQ((jet_t{expected_left_sum + expected_residual, expected_derivative}), sut(location));
}

// ====================================================================================================================
// antiderivative_builder_t
// ====================================================================================================================

struct quadrature_antiderivative_builder_t : Test
{
    using real_t        = float_t;
    using accumulator_t = real_t;
    using real_vec_t    = std::vector<real_t>;

    using move_only_id_t = std::unique_ptr<int_t>;
    using integral_t     = move_only_id_t;

    using map_t = std::flat_map<real_t, real_t>;

    struct antiderivative_t
    {
        using real_t            = float_t;
        using map_t             = map_t;
        using boundaries_t      = map_t::key_container_type;
        using cumulative_sums_t = map_t::mapped_container_type;
        using integral_t        = integral_t;

        integral_t integral;
        map_t      intervals;
    };

    using sut_t = antiderivative_builder_t<accumulator_t, antiderivative_t>;
    sut_t sut{};

    using result_t = sut_t::result_t;

    static constexpr auto expected_integral_id = 5;

    auto finalize() noexcept -> result_t
    {
        return std::move(sut).finalize(std::make_unique<int_t>(expected_integral_id));
    }
};

TEST_F(quadrature_antiderivative_builder_t, append_none)
{
    auto const actual = finalize();

    EXPECT_EQ(*actual.antiderivative.integral, expected_integral_id);
    EXPECT_EQ((map_t{{0, 0}}), actual.antiderivative.intervals);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 0.0);
    EXPECT_DOUBLE_EQ(actual.max_error, 0.0);
}

TEST_F(quadrature_antiderivative_builder_t, append_one)
{
    sut.append(1.3, 5.7, 7.11);

    auto const actual = finalize();

    EXPECT_EQ(*actual.antiderivative.integral, expected_integral_id);
    EXPECT_EQ((map_t{{0, 0}, {1.3, 5.7}}), actual.antiderivative.intervals);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 7.11);
    EXPECT_DOUBLE_EQ(actual.max_error, 7.11);
}

TEST_F(quadrature_antiderivative_builder_t, append_many)
{
    sut.append(1.3, 5.7, 7.11);
    sut.append(13.17, 17.19, 53.59); // max error does not come last
    sut.append(23.29, 31.37, 41.43);

    auto const actual = finalize();

    EXPECT_EQ(*actual.antiderivative.integral, expected_integral_id);
    EXPECT_EQ((map_t{{0, 0}, {1.3, 5.7}, {13.17, 5.7 + 17.19}, {23.29, 5.7 + 17.19 + 31.37}}),
              actual.antiderivative.intervals);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 7.11 + 53.59 + 41.43);
    EXPECT_DOUBLE_EQ(actual.max_error, 53.59);
}

} // namespace
} // namespace crv::quadrature::generic
