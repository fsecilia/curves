// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "antiderivative.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <gmock/gmock.h>

namespace crv::quadrature::generic {
namespace {

// ====================================================================================================================
// antiderivative_t
// ====================================================================================================================

struct quadrature_antiderivative_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = jet_t<scalar_t>;

    struct mock_integral_t
    {
        MOCK_METHOD(scalar_t, integrate, (scalar_t left, scalar_t right), (const, noexcept));
        MOCK_METHOD(scalar_t, average, (scalar_t left, scalar_t right), (const, noexcept));
        MOCK_METHOD(scalar_t, evaluate_integrand, (scalar_t x), (const, noexcept));
        virtual ~mock_integral_t() = default;
    };
    StrictMock<mock_integral_t> mock_integral;

    struct integral_t
    {
        using scalar_t = float_t;

        mock_integral_t* mock = nullptr;

        auto integrate(scalar_t left, scalar_t right) const noexcept -> scalar_t
        {
            return mock->integrate(left, right);
        }

        auto average(scalar_t left, scalar_t right) const noexcept -> scalar_t { return mock->average(left, right); }

        auto evaluate_integrand(scalar_t x) const noexcept -> scalar_t { return mock->evaluate_integrand(x); }
    };

    using sut_t = antiderivative_t<integral_t>;

    static constexpr auto expected_residual = 0.3174;
    static constexpr auto expected_derivative = 1.7213;
    static constexpr auto input_tangent = 2.1;

    auto test_call(sut_t const& sut, scalar_t x, scalar_t expected_left, scalar_t expected_sum) const noexcept -> void
    {
        EXPECT_CALL(mock_integral, integrate(expected_left, x)).WillOnce(Return(expected_residual));

        EXPECT_EQ(expected_sum + expected_residual, sut(x));
    }

    auto test_call(sut_t const& sut, jet_t x, scalar_t expected_left, scalar_t expected_sum) const noexcept -> void
    {
        EXPECT_CALL(mock_integral, integrate(expected_left, primal(x))).WillOnce(Return(expected_residual));
        EXPECT_CALL(mock_integral, evaluate_integrand(primal(x))).WillOnce(Return(expected_derivative));

        EXPECT_EQ((jet_t{expected_sum + expected_residual, expected_derivative * tangent(x)}), sut(x));
    }
};

// --------------------------------------------------------------------------------------------------------------------
// small cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_antiderivative_test_small_cache_t : quadrature_antiderivative_test_t
{
    sut_t sut{
        integral_t{&mock_integral},
        {0.0, 1.0, 2.0, 3.0},
        {0.0, 2.5, 5.0, 8.5},
    };

    auto test_call(auto x, scalar_t expected_left, scalar_t expected_sum) const noexcept -> void
    {
        quadrature_antiderivative_test_t::test_call(sut, x, expected_left, expected_sum);
    }
};

TEST_F(quadrature_antiderivative_test_small_cache_t, domain_end)
{
    EXPECT_EQ(3.0, sut.domain_end());
}

TEST_F(quadrature_antiderivative_test_small_cache_t, segment_count)
{
    EXPECT_EQ(3, sut.segment_count());
}

TEST_F(quadrature_antiderivative_test_small_cache_t, derivative_evaluates_retained_integrand)
{
    auto const x = 1.25;
    EXPECT_CALL(mock_integral, evaluate_integrand(x)).WillOnce(Return(expected_derivative));
    EXPECT_EQ(expected_derivative, sut.derivative(x));
}

TEST_F(quadrature_antiderivative_test_small_cache_t, mean_integrand_at_origin_is_integrand)
{
    EXPECT_CALL(mock_integral, evaluate_integrand(0.0)).WillOnce(Return(expected_derivative));
    EXPECT_EQ(expected_derivative, sut.mean_integrand(0.0));
}

TEST_F(quadrature_antiderivative_test_small_cache_t, mean_integrand_in_first_interval_is_direct_residual_mean)
{
    auto const x = 0.5;
    auto const residual_mean = 1.75;
    EXPECT_CALL(mock_integral, average(0.0, x)).WillOnce(Return(residual_mean));
    EXPECT_EQ(residual_mean, sut.mean_integrand(x));
}

TEST_F(quadrature_antiderivative_test_small_cache_t, mean_integrand_on_cache_boundary_uses_cached_prefix)
{
    EXPECT_DOUBLE_EQ(2.5, sut.mean_integrand(1.0));
}

TEST_F(
    quadrature_antiderivative_test_small_cache_t, mean_integrand_immediately_after_boundary_blends_prefix_and_residual)
{
    auto const x = std::nextafter(1.0, 2.0);
    auto const residual_mean = 7.0;
    EXPECT_CALL(mock_integral, average(1.0, x)).WillOnce(Return(residual_mean));

    auto const expected = residual_mean + (1.0 / x) * (2.5 - residual_mean);
    EXPECT_DOUBLE_EQ(expected, sut.mean_integrand(x));
}

TEST_F(
    quadrature_antiderivative_test_small_cache_t, mean_integrand_in_later_interval_blends_cached_prefix_and_local_mean)
{
    auto const x = 1.5;
    auto const residual_mean = 4.0;
    EXPECT_CALL(mock_integral, average(1.0, x)).WillOnce(Return(residual_mean));

    auto const expected = residual_mean + (1.0 / x) * (2.5 - residual_mean);
    EXPECT_DOUBLE_EQ(expected, sut.mean_integrand(x));
}

// test left edge of the domain with scalar
TEST_F(quadrature_antiderivative_test_small_cache_t, domain_min_scalar)
{
    test_call(0.0, 0.0, 0.0);
}

// test left edge of the domain with jet
TEST_F(quadrature_antiderivative_test_small_cache_t, domain_min_jet)
{
    test_call(jet_t{0.0, input_tangent}, 0.0, 0.0);
}

// tests point inside first segment with scalar, 0.0 <= x < 1.0
TEST_F(quadrature_antiderivative_test_small_cache_t, first_segment_scalar)
{
    test_call(0.5, 0.0, 0.0);
}

// tests point inside first segment with jet, 0.0 <= x < 1.0
TEST_F(quadrature_antiderivative_test_small_cache_t, first_segment_jet)
{
    test_call(jet_t{0.5, input_tangent}, 0.0, 0.0);
}

// tests point exactly on first boundary with scalar
TEST_F(quadrature_antiderivative_test_small_cache_t, first_boundary_scalar)
{
    test_call(1.0, 1.0, 2.5);
}

// tests point exactly on first boundary with jet
TEST_F(quadrature_antiderivative_test_small_cache_t, first_boundary_jet)
{
    test_call(jet_t{1.0, input_tangent}, 1.0, 2.5);
}

// test point inside a middle interval with scalar, 1.0 < x < 2.0
TEST_F(quadrature_antiderivative_test_small_cache_t, middle_interval_scalar)
{
    test_call(1.5, 1.0, 2.5);
}

// test point inside a middle interval with scalar, 1.0 < x < 2.0
TEST_F(quadrature_antiderivative_test_small_cache_t, middle_interval_jet)
{
    test_call(jet_t{1.5, input_tangent}, 1.0, 2.5);
}

// test right edge of the domain wsith scalar
TEST_F(quadrature_antiderivative_test_small_cache_t, domain_end_scalar)
{
    test_call(3.0, 3.0, 8.5);
}

// test right edge of the domain wsith jet
TEST_F(quadrature_antiderivative_test_small_cache_t, domain_end_jet)
{
    test_call(jet_t{3.0, input_tangent}, 3.0, 8.5);
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

// query scalar below the valid domain bounds
TEST_F(quadrature_antiderivative_test_small_cache_t, query_scalar_below_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut(-0.01), "domain error");
}

// query jet below the valid domain bounds
TEST_F(quadrature_antiderivative_test_small_cache_t, query_jet_below_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut(jet_t{-0.01, input_tangent}), "domain error");
}

// query scalar strictly above the valid domain bounds
TEST_F(quadrature_antiderivative_test_small_cache_t, query_scalar_above_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut(3.01), "domain error");
}

// query jet strictly above the valid domain bounds
TEST_F(quadrature_antiderivative_test_small_cache_t, query_jet_above_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut(jet_t{3.01, input_tangent}), "domain error");
}

// test scalar NaN injection
TEST_F(quadrature_antiderivative_test_small_cache_t, query_scalar_nan_aborts)
{
    EXPECT_DEBUG_DEATH(sut(std::numeric_limits<scalar_t>::quiet_NaN()), "domain error");
}

// test jet NaN injection
TEST_F(quadrature_antiderivative_test_small_cache_t, query_jet_nan_aborts)
{
    EXPECT_DEBUG_DEATH(sut(jet_t{std::numeric_limits<scalar_t>::quiet_NaN(), input_tangent}), "domain error");
}

#endif

// --------------------------------------------------------------------------------------------------------------------
// minimal cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_antiderivative_test_minimal_cache_t : quadrature_antiderivative_test_t
{
    sut_t sut{
        integral_t{&mock_integral},
        {0.0, 1.5},
        {0.0, 3.0},
    };

    auto test_call(auto x, scalar_t expected_left, scalar_t expected_sum) const noexcept -> void
    {
        quadrature_antiderivative_test_t::test_call(sut, x, expected_left, expected_sum);
    }
};

TEST_F(quadrature_antiderivative_test_minimal_cache_t, segment_count)
{
    EXPECT_EQ(1, sut.segment_count());
}

// test left edge of the domain with scalar
TEST_F(quadrature_antiderivative_test_minimal_cache_t, domain_min_scalar)
{
    test_call(0.0, 0.0, 0.0);
}

// test left edge of the domain with jet
TEST_F(quadrature_antiderivative_test_minimal_cache_t, domain_min_jet)
{
    test_call(jet_t{0.0, input_tangent}, 0.0, 0.0);
}

// test interior with scalar
TEST_F(quadrature_antiderivative_test_minimal_cache_t, interior_scalar)
{
    test_call(0.75, 0.0, 0.0);
}

// test interior with jet
TEST_F(quadrature_antiderivative_test_minimal_cache_t, interior_jet)
{
    test_call(jet_t{0.75, input_tangent}, 0.0, 0.0);
}

// test right edge of the domain with scalar
TEST_F(quadrature_antiderivative_test_minimal_cache_t, domain_end_scalar)
{
    test_call(1.5, 1.5, 3.0);
}

// test right edge of the domain with jet
TEST_F(quadrature_antiderivative_test_minimal_cache_t, domain_end_jet)
{
    test_call(jet_t{1.5, input_tangent}, 1.5, 3.0);
}

// ====================================================================================================================
// antiderivative_builder_t
// ====================================================================================================================

struct quadrature_antiderivative_builder_t : Test
{
    using scalar_t = float_t;
    using accumulator_t = scalar_t;
    using real_vec_t = std::vector<scalar_t>;

    using move_only_id_t = std::unique_ptr<int_t>;
    using integral_t = move_only_id_t;

    using boundaries_t = std::vector<scalar_t>;
    using cumulative_sums_t = std::vector<scalar_t>;

    struct antiderivative_t
    {
        using scalar_t = float_t;
        using boundaries_t = boundaries_t;
        using cumulative_sums_t = cumulative_sums_t;
        using integral_t = integral_t;

        integral_t integral;
        boundaries_t boundaries;
        cumulative_sums_t cumulative_sums;
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

    EXPECT_EQ(expected_integral_id, *actual.antiderivative.integral);
    EXPECT_EQ(boundaries_t{0}, actual.antiderivative.boundaries);
    EXPECT_EQ(cumulative_sums_t{0}, actual.antiderivative.cumulative_sums);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 0.0);
    EXPECT_DOUBLE_EQ(actual.max_error, 0.0);
    EXPECT_FALSE(actual.refinement_limited);
}

TEST_F(quadrature_antiderivative_builder_t, append_one)
{
    sut.append(1.3, 5.7, 7.11);

    auto const actual = finalize();

    EXPECT_EQ(expected_integral_id, *actual.antiderivative.integral);

    auto const expected_boundaries = boundaries_t{0.0, 1.3};
    EXPECT_EQ(expected_boundaries, actual.antiderivative.boundaries);

    auto const expected_cumulative_sums = cumulative_sums_t{0.0, 5.7};
    EXPECT_EQ(expected_cumulative_sums, actual.antiderivative.cumulative_sums);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 7.11);
    EXPECT_DOUBLE_EQ(actual.max_error, 7.11);
    EXPECT_FALSE(actual.refinement_limited);
}

TEST_F(quadrature_antiderivative_builder_t, append_many)
{
    sut.append(1.3, 5.7, 7.11);
    sut.append(13.17, 17.19, 53.59); // max error does not come last
    sut.append(23.29, 31.37, 41.43);

    auto const actual = finalize();

    EXPECT_EQ(expected_integral_id, *actual.antiderivative.integral);

    auto const expected_boundaries = boundaries_t{0, 1.3, 13.17, 23.29};
    EXPECT_EQ(expected_boundaries, actual.antiderivative.boundaries);

    auto const expected_cumulative_sums = cumulative_sums_t{0, 5.7, 5.7 + 17.19, 5.7 + 17.19 + 31.37};
    EXPECT_EQ(expected_cumulative_sums, actual.antiderivative.cumulative_sums);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 7.11 + 53.59 + 41.43);
    EXPECT_DOUBLE_EQ(actual.max_error, 53.59);
    EXPECT_FALSE(actual.refinement_limited);
}

TEST_F(quadrature_antiderivative_builder_t, retains_refinement_limit_diagnostic)
{
    sut.append(1.0, 2.0, 3.0, true);
    sut.append(2.0, 4.0, 5.0, false);

    auto const actual = finalize();

    EXPECT_TRUE(actual.refinement_limited);
}

} // namespace
} // namespace crv::quadrature::generic
