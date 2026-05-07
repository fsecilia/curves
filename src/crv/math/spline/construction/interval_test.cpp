// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "interval.hpp"
#include <crv/test/test.hpp>
#include <limits>

namespace crv::spline {
namespace {

using real_t = float_t;

struct segment_t
{
    auto operator<=>(segment_t const&) const noexcept -> auto = default;
    auto operator==(segment_t const&) const noexcept -> bool = default;
};
using sut_t = interval_t<real_t, segment_t>;

constexpr auto defects = segment_defects_t{1};
constexpr auto no_defects = segment_defects_t{0};

constexpr auto construct_sut(segment_defects_t segment_defects, real_t weighted_error, real_t left_x) noexcept -> sut_t
{
    auto result = sut_t{};
    result.left.x = left_x;
    result.residual.weighted_error = weighted_error;
    result.segment_defects = segment_defects;
    return result;
}

constexpr auto sut = interval_priority_less_t{};

// --------------------------------------------------------------------------------------------------------------------
// lexicographic dominance
// --------------------------------------------------------------------------------------------------------------------

// defects dominate weighted error
constexpr auto clean_but_huge_error = construct_sut(no_defects, 1e30, 1e30);
constexpr auto defects_but_zero_error = construct_sut(defects, 0.0, 0.0);
static_assert(sut(clean_but_huge_error, defects_but_zero_error));
static_assert(!sut(defects_but_zero_error, clean_but_huge_error));

// weighted error dominates left.x
static_assert(sut(construct_sut(defects, 0.0, 1e30), construct_sut(defects, 1.0, 0.0)));
static_assert(sut(construct_sut(no_defects, 0.0, 1e30), construct_sut(no_defects, 1.0, 0.0)));

// left.x breaks ties
static_assert(sut(construct_sut(defects, 5.0, 1.0), construct_sut(defects, 5.0, 2.0)));
static_assert(!sut(construct_sut(defects, 5.0, 2.0), construct_sut(defects, 5.0, 1.0)));

// --------------------------------------------------------------------------------------------------------------------
// defect equivalence classes
// --------------------------------------------------------------------------------------------------------------------

// strict weak ordering
constexpr auto defect_less = construct_sut(segment_defects_t{1}, 1.0, 1.0);
constexpr auto defect_greater = construct_sut(segment_defects_t{2}, 1.0, 1.0);
static_assert(!sut(defect_less, defect_greater));
static_assert(!sut(defect_greater, defect_less));

// --------------------------------------------------------------------------------------------------------------------
// nonfinite death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

namespace death_tests {

constexpr auto nan = std::numeric_limits<real_t>::quiet_NaN();
constexpr auto inf = std::numeric_limits<real_t>::infinity();

TEST(spline_interval_priority_less, death_by_lhs_residual_weighted_error)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, nan, 0.0), construct_sut(no_defects, 0.0, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_lhs_left_x)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, 0.0, inf), construct_sut(no_defects, 0.0, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_rhs_residual_weighted_error)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, 0.0, 0.0), construct_sut(no_defects, -inf, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_rhs_left_x)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(no_defects, 0.0, 0.0), construct_sut(no_defects, 0.0, nan)), "isfinite");
}

} // namespace death_tests

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
