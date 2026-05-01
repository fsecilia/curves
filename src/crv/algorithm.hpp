// SPDX-License-Identifier: MIT

/// \file
/// \brief simplified, kernel-compatible algorithm headeer
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv {

template <typename first_t, typename... remaining_t>
    requires(std::same_as<first_t, remaining_t> && ...) && (sizeof...(remaining_t) > 0)
constexpr auto max(first_t const& first, remaining_t const&... remaining) noexcept -> first_t const&
{
    auto const* result = &first;
    ((result = (*result < remaining) ? &remaining : result), ...);
    return *result;
}

template <typename first_t, typename... remaining_t>
    requires(std::same_as<first_t, remaining_t> && ...) && (sizeof...(remaining_t) > 0)
constexpr auto min(first_t const& first, remaining_t const&... rest) noexcept -> first_t const&
{
    auto const* result = &first;
    ((result = (rest < *result) ? &rest : result), ...);
    return *result;
}

} // namespace crv
