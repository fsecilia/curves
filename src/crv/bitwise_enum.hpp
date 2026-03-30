// SPDX-License-Identifier: MIT

/// \file
/// \brief opt-in bitwise operations on enums
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv {

// opt-in trait; defaults to false
template <typename enum_t> inline constexpr auto bitwise_for_enum_enabled = false;

// concept to constrain operators to opted-in enums
template <typename enum_t>
concept is_bitwise_enum = std::is_enum_v<enum_t> && bitwise_for_enum_enabled<enum_t>;

// --------------------------------------------------------------------------------------------------------------------
// Binary Operators
// --------------------------------------------------------------------------------------------------------------------

template <is_bitwise_enum enum_t> constexpr auto operator|(enum_t lhs, enum_t rhs) noexcept -> enum_t
{
    using underlying_t = std::underlying_type_t<enum_t>;
    return static_cast<enum_t>(static_cast<underlying_t>(lhs) | static_cast<underlying_t>(rhs));
}

template <is_bitwise_enum enum_t> constexpr auto operator&(enum_t lhs, enum_t rhs) noexcept -> enum_t
{
    using underlying_t = std::underlying_type_t<enum_t>;
    return static_cast<enum_t>(static_cast<underlying_t>(lhs) & static_cast<underlying_t>(rhs));
}

template <is_bitwise_enum enum_t> constexpr auto operator^(enum_t lhs, enum_t rhs) noexcept -> enum_t
{
    using underlying_t = std::underlying_type_t<enum_t>;
    return static_cast<enum_t>(static_cast<underlying_t>(lhs) ^ static_cast<underlying_t>(rhs));
}

template <is_bitwise_enum enum_t> constexpr auto operator~(enum_t val) noexcept -> enum_t
{
    using underlying_t = std::underlying_type_t<enum_t>;
    return static_cast<enum_t>(~static_cast<underlying_t>(val));
}

// --------------------------------------------------------------------------------------------------------------------
// Compound Assignment
// --------------------------------------------------------------------------------------------------------------------

template <is_bitwise_enum enum_t> constexpr auto operator|=(enum_t& lhs, enum_t rhs) noexcept -> enum_t&
{
    return lhs = lhs | rhs;
}

template <is_bitwise_enum enum_t> constexpr auto operator&=(enum_t& lhs, enum_t rhs) noexcept -> enum_t&
{
    return lhs = lhs & rhs;
}

template <is_bitwise_enum enum_t> constexpr auto operator^=(enum_t& lhs, enum_t rhs) noexcept -> enum_t&
{
    return lhs = lhs ^ rhs;
}

} // namespace crv
