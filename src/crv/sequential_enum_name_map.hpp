// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/concepts.hpp>
#include <array>
#include <optional>
#include <string_view>

namespace crv {

/// maps sequential enum values to string_view names
template <is_enum enum_t, std::size_t size> struct sequential_enum_name_map_t : std::array<std::string_view, size>
{
    template <typename... args_t>
    explicit constexpr sequential_enum_name_map_t(args_t... args) noexcept : std::array<std::string_view, size>{args...}
    {}

    /// \returns name mapped to value, or unknown if not found
    constexpr auto to_string(enum_t value) const noexcept -> std::string_view
    {
        auto const index = static_cast<std::size_t>(value);
        if (index < size) return this->operator[](index);
        return "unknown";
    }

    /// \returns value mapped to name, or std::nullopt is not found
    constexpr auto from_string(std::string_view name) const -> std::optional<enum_t>
    {
        for (auto index = 0u; index < size; ++index)
        {
            if (name == this->operator[](index)) return static_cast<enum_t>(index);
        }
        return std::nullopt;
    }
};

/// factory function to create a name map from an ordered list of strings
///
/// This would be ctad, but ctad does not support partial specialization. Functions do.
template <is_enum enum_t, typename... args_t>
constexpr auto sequential_enum_name_map(args_t... args) noexcept -> sequential_enum_name_map_t<enum_t, sizeof...(args)>
{
    return sequential_enum_name_map_t<enum_t, sizeof...(args)>{args...};
}

} // namespace crv
