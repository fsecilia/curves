// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_affine.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::shaping::transforms {
namespace {

using real_t = float_t;
using sut_t = output_affine_t<real_t>;

// identity; output matches input
constexpr auto identity = sut_t{1.0, 0.0};
static_assert(identity(-2.0) == -2.0);
static_assert(identity(0.0) == 0.0);
static_assert(identity(3.0) == 3.0);
static_assert(identity(5.0) == 5.0);

// shift down; subtracts 2 from input
constexpr auto shift_down = sut_t{1.0, -2.0};
static_assert(shift_down(-2.0) == -4.0);
static_assert(shift_down(0.0) == -2.0);
static_assert(shift_down(3.0) == 1.0);
static_assert(shift_down(5.0) == 3.0);

// shift up; adds 3 to input
constexpr auto shift_up = sut_t{1.0, 3.0};
static_assert(shift_up(-2.0) == 1.0);
static_assert(shift_up(0.0) == 3.0);
static_assert(shift_up(3.0) == 6.0);
static_assert(shift_up(5.0) == 8.0);

// scale down: divides input by 2
constexpr auto scale_down = sut_t{0.5, 0.0};
static_assert(scale_down(-2.0) == -1.0);
static_assert(scale_down(0.0) == 0.0);
static_assert(scale_down(3.0) == 1.5);
static_assert(scale_down(5.0) == 2.5);

// scale up: multiplies input by 3
constexpr auto scale_up = sut_t{3.0, 0.0};
static_assert(scale_up(-2.0) == -6.0);
static_assert(scale_up(0.0) == 0.0);
static_assert(scale_up(3.0) == 9.0);
static_assert(scale_up(5.0) == 15.0);

// combined: scales input by 2 then shifts result by 3
constexpr auto combined = sut_t{2.0, 3.0};
static_assert(combined(-2.0) == -1.0);
static_assert(combined(0.0) == 3.0);
static_assert(combined(3.0) == 9.0);
static_assert(combined(5.0) == 13.0);

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(input_affine_death_tests, invariant_scale_equals_0)
{
    EXPECT_DEBUG_DEATH((sut_t{0.0, 0.0}), "scale");
}

TEST(input_affine_death_tests, invariant_scale_less_than_0)
{
    EXPECT_DEBUG_DEATH((sut_t{-1.0, 0.0}), "scale");
}

#endif

} // namespace
} // namespace crv::shaping::transforms
