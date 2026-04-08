// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cache.hpp"
#include <crv/test/test.hpp>

namespace crv::quadrature {
namespace {

// ====================================================================================================================
// cache_t
// ====================================================================================================================

struct quadrature_cache_test_t : Test
{
    using real_t = float_t;
    using sut_t  = cache_t<real_t>;

    using interval_t = sut_t::interval_t;
};

// --------------------------------------------------------------------------------------------------------------------
// small cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_cache_test_small_cache_t : quadrature_cache_test_t
{
    sut_t sut{{
        {0.0, 0.0},
        {1.0, 2.5},
        {2.0, 5.0},
        {3.0, 8.5},
    }};
};

// test absolute left edge of the domain
TEST_F(quadrature_cache_test_small_cache_t, domain_min)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.0));
}

// tests point inside first interval, 0.0 <= x < 1.0
TEST_F(quadrature_cache_test_small_cache_t, first_interval)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.5));
}

// tests point exactly on an inner boundary
TEST_F(quadrature_cache_test_small_cache_t, inner_boundary)
{
    EXPECT_EQ((interval_t{1.0, 2.5}), sut.interval(1.0));
}

// test point inside a middle interval, 1.0 < x < 2.0
TEST_F(quadrature_cache_test_small_cache_t, middle_interval)
{
    EXPECT_EQ((interval_t{1.0, 2.5}), sut.interval(1.5));
}

// test absolute right edge of the domain
TEST_F(quadrature_cache_test_small_cache_t, domain_max)
{
    EXPECT_EQ((interval_t{3.0, 8.5}), sut.interval(3.0));
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

// test queries below the valid domain bounds
TEST_F(quadrature_cache_test_small_cache_t, query_below_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut.interval(-0.01), "domain error");
}

// test queries strictly above the valid domain bounds
TEST_F(quadrature_cache_test_small_cache_t, query_above_domain_aborts)
{
    EXPECT_DEBUG_DEATH(sut.interval(3.01), "domain error");
}

// test NaN injection
TEST_F(quadrature_cache_test_small_cache_t, query_nan_aborts)
{
    EXPECT_DEBUG_DEATH(sut.interval(std::numeric_limits<real_t>::quiet_NaN()), "domain error");
}

// --------------------------------------------------------------------------------------------------------------------
// minmal cache
// --------------------------------------------------------------------------------------------------------------------

struct quadrature_cache_test_minimal_cache_t : quadrature_cache_test_t
{
    sut_t sut{{
        {0.0, 0.0},
        {1.5, 3.0},
    }};
};

// test absolute left edge of the domain
TEST_F(quadrature_cache_test_minimal_cache_t, domain_min)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.0));
}

// test interior
TEST_F(quadrature_cache_test_minimal_cache_t, interior)
{
    EXPECT_EQ((interval_t{0.0, 0.0}), sut.interval(0.75));
}

// test absolute right edge of the domain
TEST_F(quadrature_cache_test_minimal_cache_t, domain_max)
{
    EXPECT_EQ((interval_t{1.5, 3.0}), sut.interval(1.5));
}

// ====================================================================================================================
// cache_builder_t
// ====================================================================================================================

struct cache_builder_test_t : Test
{
    using real_t        = float_t;
    using accumulator_t = real_t;

    struct cache_t
    {
        using map_t             = std::flat_map<real_t, real_t>;
        using boundaries_t      = map_t::key_container_type;
        using cumulative_sums_t = map_t::mapped_container_type;

        map_t map;
    };

    using sut_t = cache_builder_t<real_t, cache_t, accumulator_t>;

    sut_t sut{};
};

TEST_F(cache_builder_test_t, append_none)
{
    EXPECT_EQ((typename cache_t::map_t{{0, 0}}), std::move(sut).finalize().map);
}

TEST_F(cache_builder_test_t, append_one)
{
    sut.append(3.0, 5.0);
    EXPECT_EQ((typename cache_t::map_t{{0, 0}, {3.0, 5.0}}), std::move(sut).finalize().map);
}

TEST_F(cache_builder_test_t, append_many)
{
    sut.append(3.0, 5.0);
    sut.append(5.5, 2.3);
    sut.append(11.6, 1.1);
    EXPECT_EQ((typename cache_t::map_t{{0, 0}, {3.0, 5.0}, {5.5, 5.0 + 2.3}, {11.6, 5.0 + 2.3 + 1.1}}),
              std::move(sut).finalize().map);
}

TEST_F(cache_builder_test_t, nonmonotonic)
{
    EXPECT_DEBUG_DEATH(sut.append(0.0, -1.0), "monotonically increasing");
}

TEST_F(cache_builder_test_t, monotonic_nonincreasing)
{
    EXPECT_DEBUG_DEATH(sut.append(0.0, 0.0), "monotonically increasing");
}

} // namespace
} // namespace crv::quadrature
