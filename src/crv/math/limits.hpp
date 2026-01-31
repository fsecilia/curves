// SPDX-License-Identifier: MIT
/*!
    \file
    \brief simplified wrappers for numeric_limits

    numeric_limits is verbose. This module calls the same functions, but with shorter syntax.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <limits>

namespace crv {

template <typename value_t> constexpr auto epsilon() noexcept -> value_t
{
    return std::numeric_limits<value_t>::epsilon();
}

template <typename value_t> constexpr auto infinity() noexcept -> value_t
{
    return std::numeric_limits<value_t>::infinity();
}

template <typename value_t> constexpr auto min() noexcept -> value_t
{
    return std::numeric_limits<value_t>::min();
}

template <typename value_t> constexpr auto max() noexcept -> value_t
{
    return std::numeric_limits<value_t>::max();
}

} // namespace crv
