// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <tuple>

namespace crv {

//
// transform_tuple_t
//

namespace detail {

template <typename tuple_t, template <typename> class wrapper_t> struct transform_tuple_f;

template <typename... types_t, template <typename> class wrapper_t>
struct transform_tuple_f<std::tuple<types_t...>, wrapper_t>
{
    using type = std::tuple<wrapper_t<types_t>...>;
};

} // namespace detail

/// creates new tuple with transform applied to each element of the original tuple
///
/// transform_tuple_t<tuple<a, b, c>, transform_t> -> tuple<transform_t<a>, transform_t<b>, transform_t<c>>
template <typename tuple_t, template <typename> class transform_t>
using transform_tuple_t = typename detail::transform_tuple_f<tuple_t, transform_t>::type;

} // namespace crv
