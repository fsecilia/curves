// SPDX-License-Identifier: MIT
/*!
    \file
    \brief integer fundamentals

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <cassert>

namespace crv {

//! generic division; uses compiler's existing 128-bit division operator
inline auto div_u128_u64_generic(uint128_t dividend, uint64_t divisor) noexcept -> div_u128_u64_t
{
    return div_u128_u64_t{.quotient  = static_cast<uint64_t>(dividend / divisor),
                          .remainder = static_cast<uint64_t>(dividend % divisor)};
}

#if defined __x86_64__

// x64-specific implementation; uses divq directly to avoid missing 128/128 division instruction
inline auto div_u128_u64_x64(uint128_t dividend, uint64_t divisor) noexcept -> div_u128_u64_t
{
    // assert((dividend >> 64) < divisor && "division parameters will trap");

    auto const high = static_cast<uint64_t>(dividend >> 64);
    auto const low  = static_cast<uint64_t>(dividend);

    div_u128_u64_t result;
    asm volatile("divq %[divisor]"
                 : "=a"(result.quotient), "=d"(result.remainder)
                 : "d"(high), "a"(low), [divisor] "rm"(divisor)
                 : "cc");

    return result;
}

inline auto div_u128_u64(uint128_t dividend, uint64_t divisor) noexcept -> div_u128_u64_t
{
    return div_u128_u64_x64(dividend, divisor);
}

#else

inline auto div_u128_u64(uint128_t dividend, uint64_t divisor) noexcept -> div_u128_u64_t
{
    return div_u128_u64_generic(dividend, divisor);
}

#endif

} // namespace crv
