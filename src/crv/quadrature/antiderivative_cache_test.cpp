// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "antiderivative_cache.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv::quadrature {
namespace {

using scalar_t = float_t;
using sut_t = antiderivative_cache_t<scalar_t>;
using lookup_result_t = sut_t::lookup_result_t;

constexpr auto make_sut() -> sut_t
{
    return sut_t{{0.0, 1.0, 2.0, 3.0}, {0.0, 2.5, 5.0, 8.5}};
}

static_assert(make_sut().lookup(0.0) == lookup_result_t{0.0, 0.0});
static_assert(make_sut().lookup(0.5) == lookup_result_t{0.0, 0.0});
static_assert(make_sut().lookup(1.0) == lookup_result_t{1.0, 2.5});
static_assert(make_sut().lookup(1.5) == lookup_result_t{1.0, 2.5});
static_assert(make_sut().lookup(3.0) == lookup_result_t{3.0, 8.5});
static_assert(make_sut().domain_end() == 3.0);
static_assert(make_sut().segment_count() == 3);

TEST(quadrature_antiderivative_cache_test_t, before_one)
{
    auto const expected = lookup_result_t{0.0, 0.0};
    auto const actual = make_sut().lookup(std::nextafter(1.0, 0.0));

    EXPECT_EQ(expected, actual);
}

TEST(quadrature_antiderivative_cache_test_t, after_one)
{
    auto const expected = lookup_result_t{1.0, 2.5};
    auto const actual = make_sut().lookup(std::nextafter(1.0, 2.0));

    EXPECT_EQ(expected, actual);
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(quadrature_antiderivative_cache_test_t, empty_cache_aborts)
{
    EXPECT_DEBUG_DEATH(((void)sut_t{{}, {}}), "empty intervals");
}

TEST(quadrature_antiderivative_cache_test_t, mismatched_array_sizes_abort)
{
    EXPECT_DEBUG_DEATH(((void)sut_t{{0.0, 1.0}, {0.0}}), "equal sizes");
}

TEST(quadrature_antiderivative_cache_test_t, nonzero_origin_aborts)
{
    EXPECT_DEBUG_DEATH(((void)sut_t{{1.0, 2.0}, {0.0, 1.0}}), "origin must start at 0");
}

TEST(quadrature_antiderivative_cache_test_t, nonzero_initial_sum_aborts)
{
    EXPECT_DEBUG_DEATH(((void)sut_t{{0.0, 1.0}, {1.0, 2.0}}), "cumulative sum must start at 0");
}

TEST(quadrature_antiderivative_cache_test_t, duplicate_boundary_aborts)
{
    EXPECT_DEBUG_DEATH(((void)sut_t{{0.0, 1.0, 1.0}, {0.0, 1.0, 2.0}}), "strictly increasing");
}

TEST(quadrature_antiderivative_cache_test_t, descending_boundary_aborts)
{
    EXPECT_DEBUG_DEATH(((void)sut_t{{0.0, 2.0, 1.0}, {0.0, 1.0, 2.0}}), "strictly increasing");
}

TEST(quadrature_antiderivative_cache_test_t, nan_boundary_aborts)
{
    auto const nan = std::numeric_limits<scalar_t>::quiet_NaN();
    EXPECT_DEBUG_DEATH(((void)sut_t{{0.0, nan}, {0.0, 1.0}}), "strictly increasing");
}

TEST(quadrature_antiderivative_cache_test_t, below_domain_lookup_aborts)
{
    auto const sut = make_sut();
    EXPECT_DEBUG_DEATH((void)sut.lookup(-0.01), "domain error");
}

TEST(quadrature_antiderivative_cache_test_t, above_domain_lookup_aborts)
{
    auto const sut = make_sut();
    EXPECT_DEBUG_DEATH((void)sut.lookup(3.01), "domain error");
}

TEST(quadrature_antiderivative_cache_test_t, nan_lookup_aborts)
{
    auto const sut = make_sut();
    EXPECT_DEBUG_DEATH((void)sut.lookup(std::numeric_limits<scalar_t>::quiet_NaN()), "domain error");
}

#endif

} // namespace
} // namespace crv::quadrature
