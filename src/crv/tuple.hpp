// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <utility>

namespace crv::tuple {

//
// index_t
//

namespace detail {

// finds index of type_t in elements_t
template <typename type_t, typename... elements_t> constexpr auto pack_index() noexcept -> std::size_t
{
    auto index = std::size_t{0};
    auto const found = ((++index && std::same_as<type_t, elements_t>) || ...);
    return index - found;
}

// crack tuple to get pack and call pack_index
template <typename type_t, typename tuple_t> struct index_f;
template <typename type_t, typename... elements_t> struct index_f<type_t, std::tuple<elements_t...>>
{
    static constexpr auto value = pack_index<type_t, elements_t...>();
};

} // namespace detail

/// contains index of first occurance of element_t in tuple_t
template <typename element_t, typename tuple_t>
inline constexpr auto index_v = detail::index_f<element_t, tuple_t>::value;

//
// transform_t
//

namespace detail {

template <typename tuple_t, template <typename> class wrapper_t> struct transform_f;

template <typename... types_t, template <typename> class wrapper_t>
struct transform_f<std::tuple<types_t...>, wrapper_t>
{
    using type = std::tuple<wrapper_t<types_t>...>;
};

} // namespace detail

/// creates new tuple with transform applied to each element of the original tuple
///
/// transform_tuple_t<tuple<a, b, c>, op_t> -> tuple<op_t<a>, op_t<b>, op_t<c>>
template <typename tuple_t, template <typename> class op_t>
using transform_t = typename detail::transform_f<tuple_t, op_t>::type;

//
// iteration
//

/// applies op to each element in tuple: op(element)
template <typename tuple_t, typename op_t> constexpr auto for_each(tuple_t&& tuple, op_t&& op) -> op_t
{
    [&]<std::size_t... indices>(std::index_sequence<indices...>) {
        (op(std::get<indices>(std::forward<tuple_t>(tuple))), ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<tuple_t>>>{});

    return op;
}

// applies op to each pair of index and element: op(std::size_t{index}, element)
template <typename tuple_t, typename op_t> constexpr auto enumerate(tuple_t&& tuple, op_t&& op) -> op_t
{
    [&]<std::size_t... indices>(std::index_sequence<indices...>) {
        (op(std::integral_constant<std::size_t, indices>{}, std::get<indices>(std::forward<tuple_t>(tuple))), ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<tuple_t>>>{});

    return op;
}

} // namespace crv::tuple
