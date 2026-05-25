// SPDX-License-Identifier: MIT

/// \file
/// \brief reflection for enums
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/concepts.hpp>
#include <crv/sequential_enum_name_map.hpp>
#include <string_view>

namespace crv::reflection {

/// specialization point for enum reflection
///
/// This type is specialized to enable reflection for the given enum type. To enable reflection, specialize this with
/// a static constexpr member named map that supports to_string(enum) and from_string(string_view).
/// sequential_enum_name_map_t supports this directly.
template <is_enum> struct enum_t;

// \returns name mapped to value via reflection
template <is_enum enum_t> constexpr auto to_string(enum_t value) noexcept -> std::string_view
{
    return reflection::enum_t<enum_t>::map.to_string(value);
}

// \returns value mapped to name via reflection
template <is_enum enum_t> constexpr auto from_string(std::string_view name) noexcept -> std::optional<enum_t>
{
    return reflection::enum_t<enum_t>::map.from_string(name);
}

} // namespace crv::reflection
