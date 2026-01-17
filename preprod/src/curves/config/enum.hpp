// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Reflection for enums.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <array>
#include <optional>
#include <string_view>
#include <type_traits>

namespace curves {

template <typename Type>
concept Enumeration = std::is_enum_v<Type>;

template <Enumeration>
struct EnumReflection;

template <Enumeration Enum, std::size_t size>
struct SequentialNameMap {
  std::array<std::string_view, size> names;

  template <typename... Args>
  explicit constexpr SequentialNameMap(Args... args) noexcept
      : names{args...} {}

  constexpr auto to_string(Enum value) const noexcept -> std::string_view {
    const auto index = static_cast<std::size_t>(value);
    if (index < size) return names[index];
    return "unknown";
  }

  constexpr auto from_string(std::string_view name) const
      -> std::optional<Enum> {
    for (auto index = 0u; index < size; ++index) {
      if (name == names[index]) return static_cast<Enum>(index);
    }
    return std::nullopt;
  }
};

template <Enumeration Enum, typename... Args>
constexpr auto sequential_name_map(Args... args) noexcept {
  return SequentialNameMap<Enum, sizeof...(args)>{args...};
}

template <Enumeration Enum>
constexpr auto to_string(Enum value) noexcept -> std::string_view {
  return EnumReflection<Enum>::map.to_string(value);
}

template <Enumeration Enum>
constexpr auto from_string(std::string_view name) noexcept
    -> std::optional<Enum> {
  return EnumReflection<Enum>::map.from_string(name);
}

}  // namespace curves
