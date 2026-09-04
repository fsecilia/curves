// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "domain.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv::model {
namespace {

using scalar_t = float_t;
using sut_t = input_domain_t<scalar_t>;

auto const lowest = std::numeric_limits<scalar_t>::lowest();
auto const max = std::numeric_limits<scalar_t>::max();

TEST(input_domain_test_t, default_domain_is_empty)
{
    EXPECT_TRUE(sut_t{}.empty());
}

TEST(input_domain_test_t, empty_domain_contains_no_finite_input)
{
    EXPECT_FALSE(sut_t{}.contains(0.0));
}

TEST(input_domain_test_t, single_representable_point_contains_only_that_point)
{
    auto const sut = sut_t{3.0, 3.0};
    EXPECT_TRUE(sut.contains(3.0));
}

TEST(input_domain_test_t, finite_interval_contains_both_endpoints)
{
    auto const sut = sut_t{-3.0, 5.0};
    EXPECT_TRUE(sut.contains(sut.first()));
    EXPECT_TRUE(sut.contains(sut.last()));
}

TEST(input_domain_test_t, finite_interval_rejects_adjacent_outer_inputs)
{
    auto const sut = sut_t{-3.0, 5.0};
    EXPECT_FALSE(sut.contains(std::nextafter(sut.first(), lowest)));
    EXPECT_FALSE(sut.contains(std::nextafter(sut.last(), max)));
}

TEST(input_domain_test_t, representable_endpoint_encodes_mathematically_open_zero_boundary)
{
    auto const first = std::nextafter(scalar_t{0}, scalar_t{1});
    auto const sut = sut_t{first, max};
    EXPECT_FALSE(sut.contains(0.0));
}

TEST(input_domain_test_t, full_domain_spans_all_finite_inputs)
{
    auto const sut = sut_t::full();
    EXPECT_EQ(sut.first(), lowest);
    EXPECT_EQ(sut.last(), max);
}

TEST(input_domain_test_t, full_domain_contains_lowest_finite_input)
{
    EXPECT_TRUE(sut_t::full().contains(lowest));
}

TEST(input_domain_test_t, full_domain_contains_maximum_finite_input)
{
    EXPECT_TRUE(sut_t::full().contains(max));
}

TEST(input_domain_test_t, rejects_nonfinite_inputs)
{
    auto const sut = sut_t::full();
    EXPECT_FALSE(sut.contains(std::numeric_limits<scalar_t>::infinity()));
    EXPECT_FALSE(sut.contains(-std::numeric_limits<scalar_t>::infinity()));
    EXPECT_FALSE(sut.contains(std::numeric_limits<scalar_t>::quiet_NaN()));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(input_domain_test_t, rejects_nonfinite_first_endpoint)
{
    EXPECT_DEATH(static_cast<void>(sut_t{std::numeric_limits<scalar_t>::infinity(), 1.0}), "endpoints must be finite");
}

TEST(input_domain_test_t, rejects_nonfinite_last_endpoint)
{
    EXPECT_DEATH(static_cast<void>(sut_t{0.0, std::numeric_limits<scalar_t>::infinity()}), "endpoints must be finite");
}

TEST(input_domain_test_t, rejects_reversed_endpoints)
{
    EXPECT_DEATH(static_cast<void>(sut_t{2.0, 1.0}), "endpoints out of order");
}

TEST(input_domain_test_t, empty_domain_has_no_first_input)
{
    EXPECT_DEATH(static_cast<void>(sut_t{}.first()), "empty domain has no first input");
}

TEST(input_domain_test_t, empty_domain_has_no_last_input)
{
    EXPECT_DEATH(static_cast<void>(sut_t{}.last()), "empty domain has no last input");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::model
