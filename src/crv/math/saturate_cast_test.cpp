// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "saturate_cast.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

// lossless conversions
static_assert(saturate_cast<int_t>(0) == 0);
static_assert(saturate_cast<int_t>(37) == 37);
static_assert(saturate_cast<int64_t>(max<int32_t>()) == max<int32_t>());

// signed to narrower signed
static_assert(saturate_cast<int8_t>(static_cast<int16_t>(min<int8_t>()) - 1) == min<int8_t>());
static_assert(saturate_cast<int8_t>(static_cast<int16_t>(min<int8_t>())) == min<int8_t>());
static_assert(saturate_cast<int8_t>(static_cast<int16_t>(min<int8_t>()) + 1) == min<int8_t>() + 1);
static_assert(saturate_cast<int8_t>(static_cast<int16_t>(max<int8_t>()) - 1) == max<int8_t>() - 1);
static_assert(saturate_cast<int8_t>(static_cast<int16_t>(max<int8_t>())) == max<int8_t>());
static_assert(saturate_cast<int8_t>(static_cast<int16_t>(max<int8_t>()) + 1) == max<int8_t>());

// unsigned to narrower unsigned
static_assert(saturate_cast<uint8_t>(static_cast<uint16_t>(min<uint8_t>())) == min<uint8_t>());
static_assert(saturate_cast<uint8_t>(static_cast<uint16_t>(min<uint8_t>()) + 1) == min<uint8_t>() + 1);
static_assert(saturate_cast<uint8_t>(static_cast<uint16_t>(max<uint8_t>()) - 1) == max<uint8_t>() - 1);
static_assert(saturate_cast<uint8_t>(static_cast<uint16_t>(max<uint8_t>())) == max<uint8_t>());
static_assert(saturate_cast<uint8_t>(static_cast<uint16_t>(max<uint8_t>()) + 1) == max<uint8_t>());

// signed to unsigned
static_assert(saturate_cast<uint8_t>(min<int8_t>() - 1) == min<uint8_t>());
static_assert(saturate_cast<uint8_t>(min<int8_t>()) == min<uint8_t>());
static_assert(saturate_cast<uint8_t>(min<int8_t>() + 1) == min<uint8_t>());
static_assert(saturate_cast<uint8_t>(max<int8_t>() - 1) == max<int8_t>() - 1);
static_assert(saturate_cast<uint8_t>(max<int8_t>()) == max<int8_t>());
static_assert(saturate_cast<uint8_t>(max<int8_t>() + 1) == max<int8_t>() + 1);

// unsigned to signed
static_assert(saturate_cast<int8_t>(min<uint8_t>()) == min<uint8_t>());
static_assert(saturate_cast<int8_t>(min<uint8_t>() + 1) == min<uint8_t>() + 1);
static_assert(saturate_cast<int8_t>(max<uint8_t>() - 1) == max<int8_t>());
static_assert(saturate_cast<int8_t>(max<uint8_t>()) == max<int8_t>());
static_assert(saturate_cast<int8_t>(max<uint8_t>() + 1) == max<int8_t>());

} // namespace
} // namespace crv
