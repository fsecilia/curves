// SPDX-License-Identifier: MIT
/**
    \file
    \brief arg min, arg max, and their composition

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/limits.hpp>
#include <algorithm>
#include <cstdlib>
#include <ostream>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// Min/Max
// --------------------------------------------------------------------------------------------------------------------

/// tracks min@arg
template <typename arg_t, typename value_t> struct arg_min_t
{
    value_t value{max<value_t>()};
    arg_t   arg{};

    constexpr auto sample(arg_t arg, value_t value) noexcept -> void
    {
        if (value < this->value)
        {
            this->value = value;
            this->arg   = arg;
        }
    }

    friend auto operator<<(std::ostream& out, arg_min_t const& src) -> std::ostream&
    {
        return out << src.value << "@" << src.arg;
    }

    constexpr auto operator<=>(arg_min_t const&) const noexcept -> auto = default;
    constexpr auto operator==(arg_min_t const&) const noexcept -> bool  = default;
};

/// tracks max@arg
template <typename arg_t, typename value_t> struct arg_max_t
{
    value_t value{min<value_t>()};
    arg_t   arg{};

    constexpr auto sample(arg_t arg, value_t value) noexcept -> void
    {
        if (this->value < value)
        {
            this->value = value;
            this->arg   = arg;
        }
    }

    friend auto operator<<(std::ostream& out, arg_max_t const& src) -> std::ostream&
    {
        return out << src.value << "@" << src.arg;
    }

    constexpr auto operator<=>(arg_max_t const&) const noexcept -> auto = default;
    constexpr auto operator==(arg_max_t const&) const noexcept -> bool  = default;
};

/// tracks signed min and max, max magnitude
template <typename arg_t, typename value_t, typename arg_min_t = arg_min_t<arg_t, value_t>,
          typename arg_max_t = arg_max_t<arg_t, value_t>>
struct min_max_t
{
    arg_min_t min;
    arg_max_t max;

    constexpr auto max_mag() const noexcept -> value_t { return std::max(std::abs(min.value), std::abs(max.value)); }
    constexpr auto arg_max_mag() const noexcept -> arg_t
    {
        return std::abs(min.value) < std::abs(max.value) ? max.arg : min.arg;
    }

    constexpr auto sample(arg_t arg, value_t value) noexcept -> void
    {
        min.sample(arg, value);
        max.sample(arg, value);
    }

    friend auto operator<<(std::ostream& out, min_max_t const& src) -> std::ostream&
    {
        return out << "min = " << src.min << "\nmax = " << src.max;
    }

    constexpr auto operator<=>(min_max_t const&) const noexcept -> auto = default;
    constexpr auto operator==(min_max_t const&) const noexcept -> bool  = default;
};

} // namespace crv
