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

constexpr auto constant = static_t<value_t, -5, 7>{};
static_assert(constant.min == constant(min<value_t>()));
static_assert(constant.min == constant(constant.min - 1));
static_assert(constant.min == constant(constant.min));
static_assert(constant.min + 1 == constant(constant.min + 1));
static_assert(0 == none(0));
static_assert(constant.max - 1 == constant(constant.max - 1));
static_assert(constant.max == constant(constant.max));
static_assert(constant.max == constant(constant.max + 1));
static_assert(constant.max == constant(max<value_t>()));

constexpr auto dynamic = dynamic_t<value_t>{-5, 7};
static_assert(dynamic.min == dynamic(min<value_t>()));
static_assert(dynamic.min == dynamic(dynamic.min - 1));
static_assert(dynamic.min == dynamic(dynamic.min));
static_assert(dynamic.min + 1 == dynamic(dynamic.min + 1));
static_assert(0 == none(0));
static_assert(dynamic.max - 1 == dynamic(dynamic.max - 1));
static_assert(dynamic.max == dynamic(dynamic.max));
static_assert(dynamic.max == dynamic(dynamic.max + 1));
static_assert(dynamic.max == dynamic(max<value_t>()));

} // namespace
} // namespace crv::reflection::constraints
