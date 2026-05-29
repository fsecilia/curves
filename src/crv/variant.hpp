// SPDX-License-Identifier: MIT

/// \file
/// \brief conversions between sum and product types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace crv::variant {

//
// has_same_types
//

namespace detail {

template <typename tuple_t, typename variant_t> struct same_types_t : std::bool_constant<false>
{};

template <typename... elements_t>
struct same_types_t<std::tuple<elements_t...>, std::variant<elements_t...>> : std::bool_constant<true>
{};

} // namespace detail

template <typename tuple_t, typename variant_t>
concept has_same_types = detail::same_types_t<tuple_t, variant_t>::value;

} // namespace crv::variant
