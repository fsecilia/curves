// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "limits.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

static_assert(min<int8_t>() == std::numeric_limits<int8_t>::min());
static_assert(max<int8_t>() == std::numeric_limits<int8_t>::max());
static_assert(min<int64_t>() == std::numeric_limits<int64_t>::min());
static_assert(max<int64_t>() == std::numeric_limits<int64_t>::max());
static_assert(min<int128_t>() == std::numeric_limits<int128_t>::min());
static_assert(max<int128_t>() == std::numeric_limits<int128_t>::max());

static_assert(min<uint8_t>() == std::numeric_limits<uint8_t>::min());
static_assert(max<uint8_t>() == std::numeric_limits<uint8_t>::max());
static_assert(min<uint64_t>() == std::numeric_limits<uint64_t>::min());
static_assert(max<uint64_t>() == std::numeric_limits<uint64_t>::max());
static_assert(min<uint128_t>() == std::numeric_limits<uint128_t>::min());
static_assert(max<uint128_t>() == std::numeric_limits<uint128_t>::max());

} // namespace
} // namespace crv
