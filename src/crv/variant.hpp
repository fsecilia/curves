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

//
// from_variant_t
//

namespace detail {

template <typename variant_t> struct from_variant_f;
template <typename... elements_t> struct from_variant_f<std::variant<elements_t...>>
{
    using type = std::tuple<elements_t...>;
};

} // namespace detail

/// converts variant alternatives into tuple
template <typename variant_t> using from_variant_t = detail::from_variant_f<variant_t>::type;

//
// to_variant_t
//

namespace detail {

template <typename tuple_t> struct to_variant_f;
template <typename... elements_t> struct to_variant_f<std::tuple<elements_t...>>
{
    using type = std::variant<elements_t...>;
};

} // namespace detail

/// converts tuple elements into variant
template <typename tuple_t> using to_variant_t = detail::to_variant_f<tuple_t>::type;

} // namespace crv::variant
