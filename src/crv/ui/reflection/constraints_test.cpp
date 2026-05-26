// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "constraints.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::reflection::constraints {
namespace {

using value_t = int_t;

constexpr auto none = none_t{};
static_assert(min<value_t>() == none(min<value_t>()));
static_assert(0 == none(0));
static_assert(max<value_t>() == none(max<value_t>()));

constexpr auto range = range_t<value_t>{-5, 7};
static_assert(range.min == range(min<value_t>()));
static_assert(range.min == range(range.min - 1));
static_assert(range.min == range(range.min));
static_assert(range.min + 1 == range(range.min + 1));
static_assert(0 == none(0));
static_assert(range.max - 1 == range(range.max - 1));
static_assert(range.max == range(range.max));
static_assert(range.max == range(range.max + 1));
static_assert(range.max == range(max<value_t>()));

} // namespace
} // namespace crv::reflection::constraints
