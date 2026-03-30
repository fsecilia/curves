// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "bitwise_enum.hpp"
#include <crv/test/test.hpp>
#include <utility>

namespace crv {
namespace {

enum class opted_out_t
{
    value
};
static_assert(!is_bitwise_enum<opted_out_t>);

using underlying_t = int8_t;

enum class enum_t : underlying_t
{
    value_0 = 1 << 0,
    value_1 = 1 << 1,
    value_2 = 1 << 2,
};

} // namespace

template <> inline constexpr auto bitwise_for_enum_enabled<enum_t> = true;

namespace {

static_assert(is_bitwise_enum<enum_t>);

// ====================================================================================================================
// Binary Operators
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// or
// --------------------------------------------------------------------------------------------------------------------

static constexpr auto expected_or(enum_t lhs, enum_t rhs)
{
    return static_cast<enum_t>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

static_assert((enum_t::value_0 | enum_t::value_0) == expected_or(enum_t::value_0, enum_t::value_0));
static_assert((enum_t::value_0 | enum_t::value_1) == expected_or(enum_t::value_0, enum_t::value_1));
static_assert((enum_t::value_0 | enum_t::value_2) == expected_or(enum_t::value_0, enum_t::value_2));
static_assert((enum_t::value_1 | enum_t::value_2) == expected_or(enum_t::value_1, enum_t::value_2));
static_assert((enum_t::value_2 | enum_t::value_1) == expected_or(enum_t::value_2, enum_t::value_1));

// --------------------------------------------------------------------------------------------------------------------
// and
// --------------------------------------------------------------------------------------------------------------------

static constexpr auto expected_and(enum_t lhs, enum_t rhs)
{
    return static_cast<enum_t>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

static_assert((enum_t::value_0 & enum_t::value_0) == expected_and(enum_t::value_0, enum_t::value_0));
static_assert((enum_t::value_0 & enum_t::value_1) == expected_and(enum_t::value_0, enum_t::value_1));
static_assert((enum_t::value_0 & enum_t::value_2) == expected_and(enum_t::value_0, enum_t::value_2));
static_assert((enum_t::value_1 & enum_t::value_2) == expected_and(enum_t::value_1, enum_t::value_2));
static_assert((enum_t::value_2 & enum_t::value_1) == expected_and(enum_t::value_2, enum_t::value_1));

// --------------------------------------------------------------------------------------------------------------------
// xor
// --------------------------------------------------------------------------------------------------------------------

static constexpr auto expected_xor(enum_t lhs, enum_t rhs)
{
    return static_cast<enum_t>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
}

static_assert((enum_t::value_0 ^ enum_t::value_0) == expected_xor(enum_t::value_0, enum_t::value_0));
static_assert((enum_t::value_0 ^ enum_t::value_1) == expected_xor(enum_t::value_0, enum_t::value_1));
static_assert((enum_t::value_0 ^ enum_t::value_2) == expected_xor(enum_t::value_0, enum_t::value_2));
static_assert((enum_t::value_1 ^ enum_t::value_2) == expected_xor(enum_t::value_1, enum_t::value_2));
static_assert((enum_t::value_2 ^ enum_t::value_1) == expected_xor(enum_t::value_2, enum_t::value_1));

// not
static_assert(~enum_t::value_0 == static_cast<enum_t>(~std::to_underlying(enum_t::value_0)));
static_assert(~enum_t::value_1 == static_cast<enum_t>(~std::to_underlying(enum_t::value_1)));
static_assert(~enum_t::value_2 == static_cast<enum_t>(~std::to_underlying(enum_t::value_2)));

// ====================================================================================================================
// Compound Assignment
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// or
// --------------------------------------------------------------------------------------------------------------------

constexpr auto expected_or_assign(enum_t lhs, enum_t rhs) noexcept -> bool
{
    auto val = lhs;
    return &val == &(val |= rhs);
}

static_assert(expected_or_assign(enum_t::value_0, enum_t::value_0));
static_assert(expected_or_assign(enum_t::value_0, enum_t::value_1));

constexpr auto expected_or_value(enum_t lhs, enum_t rhs) noexcept -> enum_t
{
    auto val = lhs;
    val |= rhs;
    return val;
}

static_assert(expected_or_value(enum_t::value_0, enum_t::value_0)
              == static_cast<enum_t>(std::to_underlying(enum_t::value_0) | std::to_underlying(enum_t::value_0)));
static_assert(expected_or_value(enum_t::value_0, enum_t::value_1)
              == static_cast<enum_t>(std::to_underlying(enum_t::value_0) | std::to_underlying(enum_t::value_1)));

// --------------------------------------------------------------------------------------------------------------------
// and
// --------------------------------------------------------------------------------------------------------------------

constexpr auto expected_and_assign(enum_t lhs, enum_t rhs) noexcept -> bool
{
    auto val = lhs;
    return &val == &(val &= rhs);
}

static_assert(expected_and_assign(enum_t::value_0, enum_t::value_0));
static_assert(expected_and_assign(enum_t::value_0, enum_t::value_1));

constexpr auto expected_and_value(enum_t lhs, enum_t rhs) noexcept -> enum_t
{
    auto val = lhs;
    val &= rhs;
    return val;
}

static_assert(expected_and_value(enum_t::value_0, enum_t::value_0)
              == static_cast<enum_t>(std::to_underlying(enum_t::value_0) & std::to_underlying(enum_t::value_0)));
static_assert(expected_and_value(enum_t::value_0, enum_t::value_1)
              == static_cast<enum_t>(std::to_underlying(enum_t::value_0) & std::to_underlying(enum_t::value_1)));

// --------------------------------------------------------------------------------------------------------------------
// xor
// --------------------------------------------------------------------------------------------------------------------

constexpr auto expected_xor_assign(enum_t lhs, enum_t rhs) noexcept -> bool
{
    auto val = lhs;
    return &val == &(val ^= rhs);
}

static_assert(expected_xor_assign(enum_t::value_0, enum_t::value_0));
static_assert(expected_xor_assign(enum_t::value_0, enum_t::value_1));

constexpr auto expected_xor_value(enum_t lhs, enum_t rhs) noexcept -> enum_t
{
    auto val = lhs;
    val ^= rhs;
    return val;
}

static_assert(expected_xor_value(enum_t::value_0, enum_t::value_0)
              == static_cast<enum_t>(std::to_underlying(enum_t::value_0) ^ std::to_underlying(enum_t::value_0)));
static_assert(expected_xor_value(enum_t::value_0, enum_t::value_1)
              == static_cast<enum_t>(std::to_underlying(enum_t::value_0) ^ std::to_underlying(enum_t::value_1)));

} // namespace
} // namespace crv
