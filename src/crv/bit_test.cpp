// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "bit.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv {

// base case
static_assert(bit_width(0) == 0);

// small positive values
static_assert(bit_width(1) == 1);
static_assert(bit_width(2) == 2);
static_assert(bit_width(3) == 2);
static_assert(bit_width(4) == 3);
static_assert(bit_width(7) == 3);
static_assert(bit_width(8) == 4);

// small negative values
static_assert(bit_width(-1) == 1);
static_assert(bit_width(-2) == 2);
static_assert(bit_width(-3) == 2);
static_assert(bit_width(-4) == 3);
static_assert(bit_width(-7) == 3);
static_assert(bit_width(-8) == 4);

// 8-bit limits
static_assert(bit_width<int8_t>(min<int8_t>()) == 8);
static_assert(bit_width<int8_t>(min<int8_t>() + 1) == 7);
static_assert(bit_width<int8_t>(max<int8_t>()) == 7);

// 16-bit limits
static_assert(bit_width<int16_t>(min<int16_t>()) == 16);
static_assert(bit_width<int16_t>(min<int16_t>() + 1) == 15);
static_assert(bit_width<int16_t>(max<int16_t>()) == 15);

// 32-bit limits
static_assert(bit_width<int32_t>(min<int32_t>()) == 32);
static_assert(bit_width<int32_t>(min<int32_t>() + 1) == 31);
static_assert(bit_width<int32_t>(max<int32_t>()) == 31);

// 64-bit limits
static_assert(bit_width<int64_t>(min<int64_t>()) == 64);
static_assert(bit_width<int64_t>(min<int64_t>() + 1) == 63);
static_assert(bit_width<int64_t>(max<int64_t>()) == 63);

} // namespace crv
