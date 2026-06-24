// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "limit_engager.hpp"
#include <crv/test/test.hpp>

namespace crv::shaping::construction {
namespace {

using real_t = float_t;

using sut_t = limiter_engager_t<real_t>;
constexpr auto sut = sut_t{};

// engaged
static_assert(sut(10.0, 10.0, 2.0) == 1.0);
static_assert(sut(12.0, 10.0, 2.0) == 1.0);

// disengaged
static_assert(sut(5.0, 10.0, 5.0) == 0.0);
static_assert(sut(3.0, 10.0, 5.0) == 0.0);

// partially engaged
static_assert(sut(8.0, 10.0, 4.0) == 0.5);
static_assert(sut(9.0, 10.0, 4.0) == 0.75);
static_assert(sut(7.0, 10.0, 4.0) == 0.25);

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(shaping_construction_limiter_engager_death_tests, zero_height)
{
    EXPECT_DEATH(static_cast<void>(sut(0.0, 0.0, 0.0)), "transition_height.*>.*0");
}

#endif

} // namespace
} // namespace crv::shaping::construction
