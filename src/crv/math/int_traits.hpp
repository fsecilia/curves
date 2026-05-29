// SPDX-License-Identifier: MIT

/// \file
/// \brief integer traits that include 128-bit types
///
/// This module contains traits for integers. Most of it is dedicated to 128-bit types that are not included in the
/// equivalent standard library traits.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/traits.hpp>
#include <cassert>
#include <type_traits>

namespace crv {

template <typename value_t>
inline constexpr bool is_128_bit_v
    = std::is_same_v<std::remove_cv_t<value_t>, int128_t> || std::is_same_v<std::remove_cv_t<value_t>, uint128_t>;

// --------------------------------------------------------------------------------------------------------------------
// integral
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t>
struct is_integral : std::bool_constant<std::is_integral_v<value_t> || is_128_bit_v<value_t>>
{};

template <typename value_t> inline constexpr auto is_integral_v = is_integral<value_t>::value;

template <typename value_t>
concept integral = is_integral_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// arithmetic
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t>
struct is_arithmetic : std::bool_constant<is_integral_v<value_t> || std::is_floating_point_v<value_t>>
{};

template <typename value_t> inline constexpr auto is_arithmetic_v = is_arithmetic<value_t>::value;

template <typename value_t>
concept arithmetic = is_arithmetic_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// signed
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t>
inline constexpr auto is_signed_v = [] {
    if constexpr (is_arithmetic_v<value_t>) { return value_t(-1) < value_t{0}; }
    else return false;
}();

template <typename value_t> struct is_signed : std::bool_constant<is_signed_v<value_t>>
{};

template <typename value_t>
concept signed_integral = integral<value_t> && is_signed_v<value_t>;

template <typename value_t>
concept unsigned_integral = integral<value_t> && !signed_integral<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// make_signed
// --------------------------------------------------------------------------------------------------------------------

namespace detail {

template <typename src_t, bool is_128 = is_128_bit_v<src_t>> struct make_signed_f : std::make_signed<src_t>
{};

// add specialization for int128_t
template <typename src_t> struct make_signed_f<src_t, true>
{
    using type = copy_cv_t<int128_t, src_t>;
};

} // namespace detail

template <typename src_t> struct make_signed : detail::make_signed_f<src_t>
{};

template <typename src_t> using make_signed_t = make_signed<src_t>::type;

// --------------------------------------------------------------------------------------------------------------------
// make_unsigned
// --------------------------------------------------------------------------------------------------------------------

namespace detail {

template <typename src_t, bool is_128 = is_128_bit_v<src_t>> struct make_unsigned_f : std::make_unsigned<src_t>
{};

// add specialization for uint128_t
template <typename src_t> struct make_unsigned_f<src_t, true>
{
    using type = copy_cv_t<uint128_t, src_t>;
};

} // namespace detail

template <typename value_t> struct make_unsigned : detail::make_unsigned_f<value_t>
{};

template <typename value_t> using make_unsigned_t = make_unsigned<value_t>::type;

} // namespace crv
