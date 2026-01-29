// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <crv/test/typed_equal.hpp>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// log2
// --------------------------------------------------------------------------------------------------------------------

static_assert(log2<uint8_t>(1) == 0);
static_assert(log2<uint8_t>(max<uint8_t>()) == 7);

// Bottom of valid range.
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

// Top of valid range.
static_assert(log2<uint64_t>((1ULL << 62) - 1) == 61);
static_assert(log2<uint64_t>((1ULL << 62) + 0) == 62);
static_assert(log2<uint64_t>((1ULL << 62) + 1) == 62);

static_assert(log2<uint64_t>((1ULL << 63) - 1) == 62);
static_assert(log2<uint64_t>((1ULL << 63) + 0) == 63);
static_assert(log2<uint64_t>((1ULL << 63) + 1) == 63);

// Max boundary.
static_assert(log2<uint64_t>(max<uint64_t>()) == 63);

#if !defined NDEBUG

TEST(log2, asserts_on_log2_0)
{
    EXPECT_DEATH(log2(0u), "log2: domain error");
}

#endif

// ====================================================================================================================
// Conversions
// ====================================================================================================================

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

#if !defined NDEBUG

TEST(int_cast, asserts_casting_negative_to_unsigned)
{
    EXPECT_DEATH(int_cast<uint8_t>(-1), "int_cast: input out of range");
}

TEST(int_cast, asserts_casting_oor)
{
    EXPECT_DEATH(int_cast<int8_t>(max<int8_t>() + 1), "int_cast: input out of range");
}

#endif

// --------------------------------------------------------------------------------------------------------------------
// to_unsigned_abs
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<uint64_t>(to_unsigned_abs(min<int64_t>()), static_cast<uint64_t>(max<int64_t>()) + 1));
static_assert(typed_equal<uint32_t>(to_unsigned_abs(min<int32_t>()), static_cast<uint32_t>(max<int32_t>()) + 1));
static_assert(typed_equal<unsigned>(to_unsigned_abs(-1), static_cast<unsigned>(1)));
static_assert(typed_equal<unsigned>(to_unsigned_abs(0), static_cast<unsigned>(0)));
static_assert(typed_equal<unsigned>(to_unsigned_abs(1), static_cast<unsigned>(1)));
static_assert(typed_equal<uint32_t>(to_unsigned_abs(max<int32_t>()), static_cast<uint32_t>(max<int32_t>())));
static_assert(typed_equal<uint64_t>(to_unsigned_abs(max<int64_t>()), static_cast<uint64_t>(max<int64_t>())));

// --------------------------------------------------------------------------------------------------------------------
// to_signed_copysign
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<int64_t>(to_signed_copysign(static_cast<uint64_t>(max<int64_t>()) + 1, -1), min<int64_t>()));
static_assert(typed_equal<int64_t>(to_signed_copysign(static_cast<uint64_t>(max<int64_t>()), -2), -max<int64_t>()));
static_assert(typed_equal<int32_t>(to_signed_copysign(static_cast<uint32_t>(max<int32_t>()) + 1, -3), min<int32_t>()));
static_assert(typed_equal<int32_t>(to_signed_copysign(static_cast<uint32_t>(max<int32_t>()), -5), -max<int32_t>()));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(1), -7), -1));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(0), -11), 0));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(0), 0), 0));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(1), 1), 1));
static_assert(typed_equal<int32_t>(to_signed_copysign(static_cast<uint32_t>(max<int32_t>()), 2), max<int32_t>()));
static_assert(typed_equal<int64_t>(to_signed_copysign(static_cast<uint64_t>(max<int64_t>()), 5), max<int64_t>()));

#if !defined NDEBUG

TEST(to_signed_copysign, asserts_casting_below_min)
{
    EXPECT_DEATH(to_signed_copysign<uint8_t>(max<int8_t>() + 2, -1), "to_signed_copysign: input out of range");
}

TEST(to_signed_copysign, asserts_casting_above_max)
{
    EXPECT_DEATH(to_signed_copysign<uint8_t>(max<int8_t>() + 1, 1), "to_signed_copysign: input out of range");
}

#endif

} // namespace
} // namespace crv
