// SPDX-License-Identifier: MIT

/// \file
/// \brief simplified, kernel-compatible min/max
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <type_traits>

namespace crv {

template <typename value_t, int_t size, bool is_signed, bool is_float> struct min_max_by_size_t;

template <typename value_t, int_t size> struct min_max_by_size_t<value_t, size, false, false>
{
    static constexpr auto max = static_cast<value_t>(~value_t{0});
    static constexpr auto min = 0;
};

template <typename value_t, int_t size> struct min_max_by_size_t<value_t, size, true, false>
{
    static constexpr auto max
        = static_cast<value_t>(min_max_by_size_t<make_unsigned_t<value_t>, size, false, false>::max >> 1);
    static constexpr auto min = -max - 1;
};

template <typename value_t>
struct min_max_t : min_max_by_size_t<value_t, sizeof(value_t), is_signed_v<value_t>, std::is_floating_point_v<value_t>>
{};

template <typename value_t> constexpr auto min() noexcept -> value_t
{
    return min_max_t<value_t>::min;
}

template <typename value_t> constexpr auto max() noexcept -> value_t
{
    return min_max_t<value_t>::max;
}

} // namespace crv
