// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv {

/// variadic max
///
/// This function has the same semantics as the 2-arg std::max, but takes any number of args.
///
/// \returns const reference to max arg
template <typename first_t, typename... remaining_t>
    requires(std::same_as<first_t, remaining_t> && ...) && (sizeof...(remaining_t) > 0)
constexpr auto max(first_t const& first, remaining_t const&... remaining) noexcept -> first_t const&
{
    auto const* result = &first;
    ((result = (*result < remaining) ? &remaining : result), ...);
    return *result;
}

/// variadic min
///
/// This function has the same semantics as the 2-arg std::min, but takes any number of args.
///
/// \returns const reference to min arg
template <typename first_t, typename... remaining_t>
    requires(std::same_as<first_t, remaining_t> && ...) && (sizeof...(remaining_t) > 0)
constexpr auto min(first_t const& first, remaining_t const&... rest) noexcept -> first_t const&
{
    auto const* result = &first;
    ((result = (rest < *result) ? &rest : result), ...);
    return *result;
}

} // namespace crv
