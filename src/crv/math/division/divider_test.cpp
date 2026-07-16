// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "divider.hpp"
#include <crv/math/rounding_mode.hpp>
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

constexpr auto rounding_mode = rounding_modes::div::truncate;

// direct hardware branch: high word < divisor
constexpr auto direct_dividend = (uint128_t{5} << 64) | uint128_t{7};
static_assert(divide<uint128_t, uint128_t, uint64_t, 0, false>(direct_dividend, uint64_t{10}, rounding_mode)
    == direct_dividend / uint64_t{10});

// long-division branch: high word >= divisor
constexpr auto long_dividend = (uint128_t{10} << 64) | uint128_t{7};
static_assert(
    crv::division::divide<uint128_t, uint128_t, uint64_t, 0, false>(long_dividend, uint64_t{10}, rounding_mode)
    == long_dividend / uint64_t{10});

// full-width quotient
static_assert(
    crv::division::divide<uint128_t, uint128_t, uint64_t, 0, false>(max<uint128_t>(), uint64_t{1}, rounding_mode)
    == max<uint128_t>());

// shifted dividend
constexpr auto shiftable = uint128_t{1} << 100;
static_assert(crv::division::divide<uint128_t, uint128_t, uint64_t, 8, false>(shiftable, uint64_t{37}, rounding_mode)
    == (shiftable << 8) / uint64_t{37});

} // namespace
} // namespace crv::division
