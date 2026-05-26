// SPDX-License-Identifier: MIT

/// \file
/// \brief optional constraints applied during reflection
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>

namespace crv::reflection::constraints {

/// no constraint applied
struct none_t
{
    template <typename value_t> constexpr auto operator()(value_t const& value) const noexcept -> value_t
    {
        return value;
    }
};

/// constrains value between compile-time values min and max
template <typename value_t, value_t t_min, value_t t_max> struct static_t
{
    static constexpr value_t min = t_min;
    static constexpr value_t max = t_max;

    constexpr auto operator()(value_t const& val) const noexcept -> value_t { return std::clamp(val, min, max); }
};

/// constrains value between runtime values min and max
template <typename value_t> struct dynamic_t
{
    value_t min;
    value_t max;

    constexpr auto operator()(value_t const& val) const noexcept -> value_t { return std::clamp(val, min, max); }
};

} // namespace crv::reflection::constraints
