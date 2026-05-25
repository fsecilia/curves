// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct unmatched_t
{};

enum legacy_enum_t
{
    legacy_enum_value
};

enum class modern_enum_t
{
    modern_enum_value
};

static_assert(!is_enum<unmatched_t>);
static_assert(is_enum<legacy_enum_t>);
static_assert(is_enum<modern_enum_t>);

} // namespace
} // namespace crv
