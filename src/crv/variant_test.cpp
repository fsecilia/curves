// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "variant.hpp"
#include <crv/test/test.hpp>

namespace crv::variant {
namespace {

using tuple_t = std::tuple<int_t, float_t, char>;
using variant_t = std::variant<int_t, float_t, char>;

//
// has_same_types
//

static_assert(has_same_types<std::tuple<int_t, float_t>, std::variant<int_t, float_t>>);
static_assert(!has_same_types<std::tuple<int_t, float_t>, std::variant<float_t, int>>);
static_assert(!has_same_types<std::tuple<int_t, float_t>, std::variant<int_t, char>>);
static_assert(!has_same_types<std::tuple<int_t>, std::variant<int_t, float_t>>);
static_assert(!has_same_types<std::tuple<int_t, float_t>, std::tuple<int_t, float_t>>);

//
// from_variant_t
//

static_assert(std::is_same_v<from_variant_t<variant_t>, tuple_t>);

} // namespace
} // namespace crv::variant
