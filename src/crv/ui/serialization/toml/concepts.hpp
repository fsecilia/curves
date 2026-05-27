// SPDX-License-Identifier: MIT

/// \file
/// \brief concepts specific to toml
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <string_view>
#include <toml++/toml.hpp>

namespace crv::serialization::tomlpp {

template <typename type_t>
concept is_toml_primitive
    = std::integral<std::remove_cvref_t<type_t>> || std::floating_point<std::remove_cvref_t<type_t>>
    || std::convertible_to<std::remove_cvref_t<type_t>, std::string_view>;

} // namespace crv::serialization::tomlpp
