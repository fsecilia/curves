// SPDX-License-Identifier: MIT

/// \file
/// \brief extends safe cross-type integer comparisons to support 128-bit types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>

namespace crv {

template <typename lhs_t, typename rhs_t> constexpr auto cmp_equal(lhs_t lhs, rhs_t rhs) noexcept -> bool
{
    if constexpr (is_signed_v<lhs_t> == is_signed_v<rhs_t>) return lhs == rhs;
    else if constexpr (is_signed_v<lhs_t>) return lhs >= 0 && static_cast<make_unsigned_t<lhs_t>>(lhs) == rhs;
    else return rhs >= 0 && lhs == static_cast<make_unsigned_t<rhs_t>>(rhs);
}

template <typename lhs_t, typename rhs_t> constexpr auto cmp_less(lhs_t lhs, rhs_t rhs) noexcept -> bool
{
    if constexpr (is_signed_v<lhs_t> == is_signed_v<rhs_t>) return lhs < rhs;
    else if constexpr (is_signed_v<lhs_t>) return lhs < 0 || static_cast<make_unsigned_t<lhs_t>>(lhs) < rhs;
    else return rhs >= 0 && lhs < static_cast<make_unsigned_t<rhs_t>>(rhs);
}

template <typename lhs_t, typename rhs_t> constexpr auto cmp_greater(lhs_t lhs, rhs_t rhs) noexcept -> bool
{
    return cmp_less(rhs, lhs);
}

template <typename lhs_t, typename rhs_t> constexpr auto cmp_less_equal(lhs_t lhs, rhs_t rhs) noexcept -> bool
{
    return !cmp_less(rhs, lhs);
}

template <typename lhs_t, typename rhs_t> constexpr auto cmp_greater_equal(lhs_t lhs, rhs_t rhs) noexcept -> bool
{
    return !cmp_less(lhs, rhs);
}

} // namespace crv
