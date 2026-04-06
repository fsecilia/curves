// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "integral_cache.hpp"
#include <crv/test/test.hpp>

namespace crv::quadrature {
namespace {

struct quadrature_integral_cache_test_t : Test
{
    using real_t = float_t;
    using sut_t  = integral_cache_t<real_t>;

    using interval_t = sut_t::interval_t;
};

// --------------------------------------------------------------------------------------------------------------------
// small cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_integral_cache_test_small_cache_t : quadrature_integral_cache_test_t
{
    sut_t sut{{
        {0.0, 0.0},
        {1.0, 2.5},
        {2.0, 5.0},
        {3.0, 8.5},
    }};
};

// test absolute left edge of the domain
TEST_F(quadrature_integral_cache_test_small_cache_t, domain_min)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.0));
}

// tests point inside first interval, 0.0 <= x < 1.0
TEST_F(quadrature_integral_cache_test_small_cache_t, first_interval)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.5));
}

// tests point exactly on an inner boundary
TEST_F(quadrature_integral_cache_test_small_cache_t, inner_boundary)
{
    EXPECT_EQ((interval_t{1.0, 2.5}), sut.interval(1.0));
}

// test point inside a middle interval, 1.0 < x < 2.0
TEST_F(quadrature_integral_cache_test_small_cache_t, middle_interval)
{
    EXPECT_EQ((interval_t{1.0, 2.5}), sut.interval(1.5));
}

// test absolute right edge of the domain
TEST_F(quadrature_integral_cache_test_small_cache_t, domain_max)
{
    EXPECT_EQ((interval_t{3.0, 8.5}), sut.interval(3.0));
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#ifndef NDEBUG

// test queries below the valid domain bounds
TEST_F(quadrature_integral_cache_test_small_cache_t, query_below_domain_aborts)
{
    EXPECT_DEATH(sut.interval(-0.01), "domain error");
}

// test queries strictly above the valid domain bounds
TEST_F(quadrature_integral_cache_test_small_cache_t, query_above_domain_aborts)
{
    EXPECT_DEATH(sut.interval(3.01), "domain error");
}

// test NaN injection
TEST_F(quadrature_integral_cache_test_small_cache_t, query_nan_aborts)
{
    EXPECT_DEATH(sut.interval(std::numeric_limits<real_t>::quiet_NaN()), "domain error");
}

#endif

// --------------------------------------------------------------------------------------------------------------------
// minmal cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_integral_cache_test_minimal_cache_t : quadrature_integral_cache_test_t
{
    sut_t sut{{
        {0.0, 0.0},
        {1.5, 3.0},
    }};
};

// test absolute left edge of the domain
TEST_F(quadrature_integral_cache_test_minimal_cache_t, domain_min)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.0));
}

// test interior
TEST_F(quadrature_integral_cache_test_minimal_cache_t, interior)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.75));
}

// test absolute right edge of the domain
TEST_F(quadrature_integral_cache_test_minimal_cache_t, domain_max)
{
    EXPECT_EQ((interval_t{1.5, 3.0}), sut.interval(1.5));
}

} // namespace
} // namespace crv::quadrature
