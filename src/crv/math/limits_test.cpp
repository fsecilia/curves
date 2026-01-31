// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "limits.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

template <typename value_t> constexpr auto test_type_limits() noexcept -> void
{
    static_assert(epsilon<value_t>() == std::numeric_limits<value_t>::epsilon());
    static_assert(infinity<value_t>() == std::numeric_limits<value_t>::infinity());
    static_assert(min<value_t>() == std::numeric_limits<value_t>::min());
    static_assert(max<value_t>() == std::numeric_limits<value_t>::max());
}

template <typename... values_t> constexpr auto test_types_limits() noexcept -> void
{
    (..., test_type_limits<values_t>());
}

template <typename... values_t> constexpr auto test_all_types_limits() noexcept -> void
{
    test_types_limits<int8_t, int64_t, int128_t, uint8_t, uint64_t, uint128_t>();
}

} // namespace
} // namespace crv
