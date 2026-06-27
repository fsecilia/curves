// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <type_traits>

namespace crv {

/// \returns true if lhs and rhs have both the expected type and share the same value.
template <typename expected_t> constexpr auto typed_equal(auto&& lhs, auto&& rhs) noexcept -> bool
{
    if constexpr (!std::same_as<expected_t, std::remove_cvref_t<decltype(lhs)>>
        || !std::same_as<expected_t, std::remove_cvref_t<decltype(rhs)>>)
    {
        return false;
    }
    else
    {
        return lhs == rhs;
    }
}

} // namespace crv
