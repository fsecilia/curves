// SPDX-License-Identifier: MIT
/*!
    \file
    \brief division-specific concepts

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <type_traits>

namespace crv::division {

// --------------------------------------------------------------------------------------------------------------------
// is_result
// --------------------------------------------------------------------------------------------------------------------

template <typename result_t> struct is_result_f : std::false_type
{};

template <typename quotient_t, typename remainder_t>
struct is_result_f<result_t<quotient_t, remainder_t>> : std::true_type
{};

template <typename result_t> inline constexpr auto is_result_v = is_result_f<result_t>::value;

template <typename result_t>
concept is_result = is_result_v<std::remove_cvref_t<result_t>>;

// --------------------------------------------------------------------------------------------------------------------
// is_divider
// --------------------------------------------------------------------------------------------------------------------

template <typename divider_t, typename dividend_t, typename divisor_t>
concept is_divider = requires(divider_t const& divider, dividend_t dividend, divisor_t divisor) {
    { divider(dividend, divisor) } -> is_result;
};

} // namespace crv::division
