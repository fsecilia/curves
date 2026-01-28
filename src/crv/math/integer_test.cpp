// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/integer.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// to_unsigned_abs
// --------------------------------------------------------------------------------------------------------------------

static_assert(to_unsigned_abs(INT64_MIN) == static_cast<uint64_t>(INT64_MAX) + 1);
static_assert(to_unsigned_abs(INT32_MIN) == static_cast<uint32_t>(INT32_MAX) + 1);
static_assert(to_unsigned_abs(-1) == 1);
static_assert(to_unsigned_abs(0) == 0);
static_assert(to_unsigned_abs(1) == 1);
static_assert(to_unsigned_abs(INT32_MAX) == static_cast<uint32_t>(INT32_MAX));
static_assert(to_unsigned_abs(INT64_MAX) == static_cast<uint64_t>(INT64_MAX));

} // namespace
} // namespace crv
