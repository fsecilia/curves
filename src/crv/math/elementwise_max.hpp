// SPDX-License-Identifier: MIT

/// \file
/// \brief cpo for cascading max applied memberwise
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>

namespace crv::cpo {

namespace elementwise_max_detail {

// poison pill: intentionally mismatches arity so it can never be selected, but supplies valid identifier name
void elementwise_max() = delete;

// matches types that already have adl-callable elementwise_max
//
// must be defined before cpo or it recurses
template <typename value_t>
concept has_adl_elementwise_max = requires(value_t const& lhs, value_t const& rhs) {
    { elementwise_max(lhs, rhs) } -> std::same_as<value_t>;
};

// matches types that have adl-callable max
template <typename value_t>
concept has_max = requires(value_t const& lhs, value_t const& rhs) {
    { max(lhs, rhs) } -> std::convertible_to<value_t>;
};

// matches types compatible with elementwise_max
template <typename value_t>
concept elementwise_max_callable = has_adl_elementwise_max<value_t> || has_max<value_t>;

// cpo
struct fn_t
{
    template <elementwise_max_callable value_t>
    constexpr auto operator()(value_t const& left, value_t const& right) const noexcept(noexcept(max(left, right)))
        -> value_t
    {
        if constexpr (has_adl_elementwise_max<value_t>) return elementwise_max(left, right);
        else return max(left, right);
    }
};

} // namespace elementwise_max_detail

/// cascading max applied member by member
///
/// For scalars, this operation has the same effect as max(), but it returns a copy. For composite types, it applies max
/// to each member and returns the aggregate of max values.
///
/// In other contexts, the operation might be called join, least upper bound, or supremum.
inline constexpr auto elementwise_max = elementwise_max_detail::fn_t{};

} // namespace crv::cpo
