// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/integer.hpp>
#include <crv/test/test.hpp>
#include <limits>

namespace crv {
namespace {

template <typename expected_t> constexpr auto typed_equal(auto&& lhs, auto&& rhs) noexcept -> bool
{
    return std::same_as<expected_t, std::remove_cvref_t<decltype(lhs)>>
           && std::same_as<expected_t, std::remove_cvref_t<decltype(rhs)>> && lhs == rhs;
}

// --------------------------------------------------------------------------------------------------------------------
// int_cast
// --------------------------------------------------------------------------------------------------------------------

static_assert(int_cast<int8_t>(int16_t{std::numeric_limits<int8_t>::min()}) == std::numeric_limits<int8_t>::min());
static_assert(int_cast<int8_t>(int16_t{-1}) == -1);
static_assert(int_cast<int8_t>(int16_t{0}) == 0);
static_assert(int_cast<int8_t>(int16_t{1}) == 1);
static_assert(int_cast<int8_t>(int16_t{std::numeric_limits<int8_t>::max()}) == std::numeric_limits<int8_t>::max());

static_assert(int_cast<int8_t>(uint16_t{0}) == 0);
static_assert(int_cast<int8_t>(uint16_t{1}) == 1);
static_assert(int_cast<int8_t>(uint16_t{std::numeric_limits<int8_t>::max()}) == std::numeric_limits<int8_t>::max());

static_assert(int_cast<int16_t>(int8_t{std::numeric_limits<int8_t>::min()}) == std::numeric_limits<int8_t>::min());
static_assert(int_cast<int16_t>(int8_t{-1}) == -1);
static_assert(int_cast<int16_t>(int8_t{0}) == 0);
static_assert(int_cast<int16_t>(int8_t{1}) == 1);
static_assert(int_cast<int16_t>(int8_t{std::numeric_limits<int8_t>::max()}) == std::numeric_limits<int8_t>::max());

static_assert(int_cast<int16_t>(uint8_t{0}) == 0);
static_assert(int_cast<int16_t>(uint8_t{1}) == 1);
static_assert(int_cast<int16_t>(uint8_t{std::numeric_limits<int8_t>::max()}) == std::numeric_limits<int8_t>::max());

#if !defined NDEBUG

TEST(int_cast, asserts_casting_negative_to_unsigned)
{
    EXPECT_DEATH(int_cast<uint8_t>(-1), "int_cast: input out of range");
}

TEST(int_cast, asserts_casting_oor)
{
    EXPECT_DEATH(int_cast<int8_t>(std::numeric_limits<int8_t>::max() + 1), "int_cast: input out of range");
}

#endif

// --------------------------------------------------------------------------------------------------------------------
// to_unsigned_abs
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<uint64_t>(to_unsigned_abs(INT64_MIN), static_cast<uint64_t>(INT64_MAX) + 1));
static_assert(typed_equal<uint32_t>(to_unsigned_abs(INT32_MIN), static_cast<uint32_t>(INT32_MAX) + 1));
static_assert(typed_equal<unsigned>(to_unsigned_abs(-1), static_cast<unsigned>(1)));
static_assert(typed_equal<unsigned>(to_unsigned_abs(0), static_cast<unsigned>(0)));
static_assert(typed_equal<unsigned>(to_unsigned_abs(1), static_cast<unsigned>(1)));
static_assert(typed_equal<uint32_t>(to_unsigned_abs(INT32_MAX), static_cast<uint32_t>(INT32_MAX)));
static_assert(typed_equal<uint64_t>(to_unsigned_abs(INT64_MAX), static_cast<uint64_t>(INT64_MAX)));

} // namespace
} // namespace crv
