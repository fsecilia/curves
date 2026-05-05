// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "integer.hpp"
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

// --------------------------------------------------------------------------------------------------------------------
// types by size
// --------------------------------------------------------------------------------------------------------------------

namespace int_by_bytes_tests {

static_assert(std::same_as<int8_t, int_by_bytes_t<1, true>>);
static_assert(std::same_as<int16_t, int_by_bytes_t<2, true>>);
static_assert(std::same_as<int32_t, int_by_bytes_t<4, true>>);
static_assert(std::same_as<int64_t, int_by_bytes_t<8, true>>);
static_assert(std::same_as<int128_t, int_by_bytes_t<16, true>>);

static_assert(std::same_as<uint8_t, int_by_bytes_t<1, false>>);
static_assert(std::same_as<uint16_t, int_by_bytes_t<2, false>>);
static_assert(std::same_as<uint32_t, int_by_bytes_t<4, false>>);
static_assert(std::same_as<uint64_t, int_by_bytes_t<8, false>>);
static_assert(std::same_as<uint128_t, int_by_bytes_t<16, false>>);

} // namespace int_by_bytes_tests

namespace int_by_bits_tests {

static_assert(std::same_as<int8_t, int_by_bits_t<1, true>>);
static_assert(std::same_as<int8_t, int_by_bits_t<8, true>>);
static_assert(std::same_as<int16_t, int_by_bits_t<9, true>>);
static_assert(std::same_as<int16_t, int_by_bits_t<16, true>>);
static_assert(std::same_as<int32_t, int_by_bits_t<17, true>>);
static_assert(std::same_as<int32_t, int_by_bits_t<32, true>>);
static_assert(std::same_as<int64_t, int_by_bits_t<33, true>>);
static_assert(std::same_as<int64_t, int_by_bits_t<64, true>>);
static_assert(std::same_as<int128_t, int_by_bits_t<65, true>>);
static_assert(std::same_as<int128_t, int_by_bits_t<128, true>>);

static_assert(std::same_as<uint8_t, int_by_bits_t<1, false>>);
static_assert(std::same_as<uint8_t, int_by_bits_t<8, false>>);
static_assert(std::same_as<uint16_t, int_by_bits_t<9, false>>);
static_assert(std::same_as<uint16_t, int_by_bits_t<16, false>>);
static_assert(std::same_as<uint32_t, int_by_bits_t<17, false>>);
static_assert(std::same_as<uint32_t, int_by_bits_t<32, false>>);
static_assert(std::same_as<uint64_t, int_by_bits_t<33, false>>);
static_assert(std::same_as<uint64_t, int_by_bits_t<64, false>>);
static_assert(std::same_as<uint128_t, int_by_bits_t<65, false>>);
static_assert(std::same_as<uint128_t, int_by_bits_t<128, false>>);

} // namespace int_by_bits_tests

// --------------------------------------------------------------------------------------------------------------------
// promotions
// --------------------------------------------------------------------------------------------------------------------

namespace integer_promotion_tests {

// can widen
static_assert(can_widen<int8_t>);
static_assert(can_widen<int16_t>);
static_assert(can_widen<int32_t>);
static_assert(can_widen<int64_t>);
static_assert(!can_widen<int128_t>);
static_assert(can_widen<uint8_t>);
static_assert(can_widen<uint16_t>);
static_assert(can_widen<uint32_t>);
static_assert(can_widen<uint64_t>);
static_assert(!can_widen<uint128_t>);

// widened
static_assert(std::same_as<widened_t<int8_t>, int16_t>);
static_assert(std::same_as<widened_t<int16_t>, int32_t>);
static_assert(std::same_as<widened_t<int32_t>, int64_t>);
static_assert(std::same_as<widened_t<int64_t>, int128_t>);
static_assert(std::same_as<widened_t<uint8_t>, uint16_t>);
static_assert(std::same_as<widened_t<uint16_t>, uint32_t>);
static_assert(std::same_as<widened_t<uint32_t>, uint64_t>);
static_assert(std::same_as<widened_t<uint64_t>, uint128_t>);

// widen 8->16
static_assert(typed_equal<int16_t>(widen(int8_t{min<int8_t>()}), int16_t{min<int8_t>()}));
static_assert(typed_equal<int16_t>(widen(int8_t{0}), int16_t{0}));
static_assert(typed_equal<int16_t>(widen(int8_t{max<int8_t>()}), int16_t{max<int8_t>()}));
static_assert(typed_equal<uint16_t>(widen(uint8_t{0}), uint16_t{0}));
static_assert(typed_equal<uint16_t>(widen(uint8_t{max<uint8_t>()}), uint16_t{max<uint8_t>()}));

// widen 64->128
static_assert(typed_equal<int128_t>(widen(int64_t{min<int64_t>()}), int128_t{min<int64_t>()}));
static_assert(typed_equal<int128_t>(widen(int64_t{0}), int128_t{0}));
static_assert(typed_equal<int128_t>(widen(int64_t{max<int64_t>()}), int128_t{max<int64_t>()}));
static_assert(typed_equal<uint128_t>(widen(uint64_t{0}), uint128_t{0}));
static_assert(typed_equal<uint128_t>(widen(uint64_t{max<uint64_t>()}), uint128_t{max<uint64_t>()}));

// can kw
static_assert(!can_narrow<int8_t>);
static_assert(can_narrow<int16_t>);
static_assert(can_narrow<int32_t>);
static_assert(can_narrow<int64_t>);
static_assert(can_narrow<int128_t>);
static_assert(!can_narrow<uint8_t>);
static_assert(can_narrow<uint16_t>);
static_assert(can_narrow<uint32_t>);
static_assert(can_narrow<uint64_t>);
static_assert(can_narrow<uint128_t>);

// narrowed
static_assert(std::same_as<narrowed_t<int16_t>, int8_t>);
static_assert(std::same_as<narrowed_t<int32_t>, int16_t>);
static_assert(std::same_as<narrowed_t<int64_t>, int32_t>);
static_assert(std::same_as<narrowed_t<int128_t>, int64_t>);
static_assert(std::same_as<narrowed_t<uint16_t>, uint8_t>);
static_assert(std::same_as<narrowed_t<uint32_t>, uint16_t>);
static_assert(std::same_as<narrowed_t<uint64_t>, uint32_t>);
static_assert(std::same_as<narrowed_t<uint128_t>, uint64_t>);

// narrow 128->64
static_assert(typed_equal<int64_t>(narrow(int128_t{min<int64_t>()}), int64_t{min<int64_t>()}));
static_assert(typed_equal<int64_t>(narrow(int128_t{0}), int64_t{0}));
static_assert(typed_equal<int64_t>(narrow(int128_t{max<int64_t>()}), int64_t{max<int64_t>()}));
static_assert(typed_equal<uint64_t>(narrow(uint128_t{0}), uint64_t{0}));
static_assert(typed_equal<uint64_t>(narrow(uint128_t{max<uint64_t>()}), uint64_t{max<uint64_t>()}));

// narrow 16->8
static_assert(typed_equal<int8_t>(narrow(int16_t{min<int8_t>()}), int8_t{min<int8_t>()}));
static_assert(typed_equal<int8_t>(narrow(int16_t{0}), int8_t{0}));
static_assert(typed_equal<int8_t>(narrow(int16_t{max<int8_t>()}), int8_t{max<int8_t>()}));
static_assert(typed_equal<uint8_t>(narrow(uint16_t{0}), uint8_t{0}));
static_assert(typed_equal<uint8_t>(narrow(uint16_t{max<uint8_t>()}), uint8_t{max<uint8_t>()}));

} // namespace integer_promotion_tests

} // namespace
} // namespace crv
