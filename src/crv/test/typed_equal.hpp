// SPDX-License-Identifier: MIT
/*!
    \file
    \brief integer fundamentals

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <type_traits>

namespace crv {

template <typename expected_t> constexpr auto typed_equal(auto&& lhs, auto&& rhs) noexcept -> bool
{
    return std::same_as<expected_t, std::remove_cvref_t<decltype(lhs)>>
           && std::same_as<expected_t, std::remove_cvref_t<decltype(rhs)>> && lhs == rhs;
}

} // namespace crv
