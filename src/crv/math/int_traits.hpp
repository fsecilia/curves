// SPDX-License-Identifier: MIT
/*!
  \file
  \brief 128-bit integer types and related concepts

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <type_traits>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// integral
// --------------------------------------------------------------------------------------------------------------------

/*
    gcc's implementation of std::is_integral is not specialized for 128-bit types. This defers to std::is_integral_v,
    extending it to include 128-bit types.
*/
template <typename value_t>
struct is_integral
    : std::bool_constant<std::is_integral_v<value_t> || std::is_same_v<std::remove_cv_t<value_t>, int128_t>
                         || std::is_same_v<std::remove_cv_t<value_t>, uint128_t>>
{};

template <typename value_t> constexpr auto is_integral_v = is_integral<value_t>::value;

template <typename value_t>
concept integral = is_integral_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// arithmetic
// --------------------------------------------------------------------------------------------------------------------

/*
    gcc's implementation of std::is_arithmetic is not specialized for 128-bit types. This defers to
    std::is_arithmetic_v, extending it to include 128-bit types.
*/
template <typename value_t>
struct is_arithmetic : std::bool_constant<is_integral_v<value_t> || std::is_floating_point_v<value_t>>
{};

template <typename value_t> constexpr auto is_arithmetic_v = is_arithmetic<value_t>::value;

template <typename value_t>
concept arithmetic = is_arithmetic_v<value_t>;

} // namespace crv
