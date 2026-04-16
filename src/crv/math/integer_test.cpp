// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "integer.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <crv/test/typed_equal.hpp>

namespace crv {
namespace {

// ====================================================================================================================
// Math
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// log2
// --------------------------------------------------------------------------------------------------------------------

static_assert(log2<uint8_t>(1) == 0);
static_assert(log2<uint8_t>(max<uint8_t>()) == 7);

// bottom of valid range
static_assert(log2<uint64_t>((1ULL << 0) + 0) == 0);
static_assert(log2<uint64_t>((1ULL << 0) + 1) == 1);

static_assert(log2<uint64_t>((1ULL << 1) - 1) == 0);
static_assert(log2<uint64_t>((1ULL << 1) + 0) == 1);
static_assert(log2<uint64_t>((1ULL << 1) + 1) == 1);

static_assert(log2<uint64_t>((1ULL << 2) - 1) == 1);
static_assert(log2<uint64_t>((1ULL << 2) + 0) == 2);
static_assert(log2<uint64_t>((1ULL << 2) + 1) == 2);

static_assert(log2<uint64_t>((1ULL << 3) - 1) == 2);
static_assert(log2<uint64_t>((1ULL << 3) + 0) == 3);
static_assert(log2<uint64_t>((1ULL << 3) + 1) == 3);

// top of valid range
static_assert(log2<uint64_t>((1ULL << 62) - 1) == 61);
static_assert(log2<uint64_t>((1ULL << 62) + 0) == 62);
static_assert(log2<uint64_t>((1ULL << 62) + 1) == 62);

static_assert(log2<uint64_t>((1ULL << 63) - 1) == 62);
static_assert(log2<uint64_t>((1ULL << 63) + 0) == 63);
static_assert(log2<uint64_t>((1ULL << 63) + 1) == 63);

// max boundary
static_assert(log2<uint64_t>(max<uint64_t>()) == 63);

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(log2, asserts_on_log2_0)
{
    EXPECT_DEBUG_DEATH(log2(0u), "log2: domain error");
}

#endif

// ====================================================================================================================
// Conversions
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// in_range
// --------------------------------------------------------------------------------------------------------------------

static_assert(in_range<int8_t>(int128_t{min<int8_t>()}));
static_assert(in_range<int8_t>(int128_t{max<int8_t>()}));
static_assert(in_range<int8_t>(uint128_t{0}));
static_assert(in_range<int8_t>(uint128_t{max<int8_t>()}));

static_assert(in_range<uint8_t>(int128_t{min<uint8_t>()}));
static_assert(in_range<uint8_t>(int128_t{max<uint8_t>()}));
static_assert(in_range<uint8_t>(uint128_t{min<uint8_t>()}));
static_assert(in_range<uint8_t>(uint128_t{max<uint8_t>()}));

static_assert(in_range<int128_t>(int8_t{min<int8_t>()}));
static_assert(in_range<int128_t>(int8_t{max<int8_t>()}));
static_assert(in_range<int128_t>(uint8_t{min<uint8_t>()}));
static_assert(in_range<int128_t>(uint8_t{max<uint8_t>()}));

static_assert(in_range<uint128_t>(int8_t{0}));
static_assert(in_range<uint128_t>(int8_t{max<int8_t>()}));
static_assert(in_range<uint128_t>(uint8_t{min<uint8_t>()}));
static_assert(in_range<uint128_t>(uint8_t{max<uint8_t>()}));

// --------------------------------------------------------------------------------------------------------------------
// int_cast
// --------------------------------------------------------------------------------------------------------------------

static_assert(int_cast<int8_t>(int16_t{min<int8_t>()}) == min<int8_t>());
static_assert(int_cast<int8_t>(int16_t{-1}) == -1);
static_assert(int_cast<int8_t>(int16_t{0}) == 0);
static_assert(int_cast<int8_t>(int16_t{1}) == 1);
static_assert(int_cast<int8_t>(int16_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int8_t>(uint16_t{0}) == 0);
static_assert(int_cast<int8_t>(uint16_t{1}) == 1);
static_assert(int_cast<int8_t>(uint16_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int16_t>(int8_t{min<int8_t>()}) == min<int8_t>());
static_assert(int_cast<int16_t>(int8_t{-1}) == -1);
static_assert(int_cast<int16_t>(int8_t{0}) == 0);
static_assert(int_cast<int16_t>(int8_t{1}) == 1);
static_assert(int_cast<int16_t>(int8_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int16_t>(uint8_t{0}) == 0);
static_assert(int_cast<int16_t>(uint8_t{1}) == 1);
static_assert(int_cast<int16_t>(uint8_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int8_t>(int128_t{max<int8_t>()}) == max<int8_t>());
static_assert(int_cast<int8_t>(uint128_t{max<int8_t>()}) == max<int8_t>());
static_assert(int_cast<uint8_t>(int128_t{max<uint8_t>()}) == max<uint8_t>());
static_assert(int_cast<uint8_t>(uint128_t{max<uint8_t>()}) == max<uint8_t>());

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(int_cast, asserts_casting_negative_to_unsigned)
{
    EXPECT_DEBUG_DEATH(int_cast<uint8_t>(-1), "int_cast: input out of range");
}

TEST(int_cast, asserts_casting_oor)
{
    EXPECT_DEBUG_DEATH(int_cast<int8_t>(max<int8_t>() + 1), "int_cast: input out of range");
}

#endif

} // namespace
} // namespace crv
