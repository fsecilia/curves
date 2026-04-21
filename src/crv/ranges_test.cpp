// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "ranges.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <list>
#include <span>
#include <string_view>

namespace crv {
namespace {

struct incompatible_t
{};

// contiguous container
using array_t = std::array<int32_t, 5>;
static_assert(compatible_range<array_t, int32_t>);
static_assert(compatible_range<array_t, int64_t>);
static_assert(!compatible_range<array_t, incompatible_t>);
static_assert(same_range<array_t, int32_t>);
static_assert(!same_range<array_t, int64_t>);
static_assert(!same_range<array_t, incompatible_t>);

// noncontiguous container
using list_t = std::list<int32_t>;
static_assert(compatible_range<list_t, int32_t>);
static_assert(compatible_range<list_t, int64_t>);
static_assert(!compatible_range<list_t, incompatible_t>);
static_assert(same_range<list_t, int32_t>);
static_assert(!same_range<list_t, int64_t>);
static_assert(!same_range<list_t, incompatible_t>);

// span
using span_t = std::span<int32_t, 3>;
static_assert(compatible_range<span_t, int32_t>);
static_assert(compatible_range<span_t, int64_t>);
static_assert(!compatible_range<span_t, incompatible_t>);
static_assert(same_range<span_t, int32_t>);
static_assert(!same_range<span_t, int64_t>);
static_assert(!same_range<span_t, incompatible_t>);

// cv-qualifiers and reference types
static_assert(compatible_range<array_t const, int32_t>);
static_assert(same_range<array_t&&, int32_t>);
static_assert(same_range<span_t const&, int32_t>);

// non-range types
static_assert(!compatible_range<int32_t, int32_t>);
static_assert(!same_range<int32_t, int32_t>);

// views
using string_view_t = std::string_view;
static_assert(compatible_range<string_view_t, char>);
static_assert(same_range<string_view_t, char>);

// unidirectional conversion test
struct base_t
{};
struct derived_t : base_t
{};
using derived_list_t = std::list<derived_t*>;
using base_list_t = std::list<base_t*>;
static_assert(compatible_range<derived_list_t, base_t*>);
static_assert(!compatible_range<base_list_t, derived_t*>);

} // namespace
} // namespace crv
