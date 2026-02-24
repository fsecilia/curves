// SPDX-License-Identifier: MIT
/*!
    \file
    \brief integer fundamentals

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/int_traits.hpp>
#include <cassert>

namespace crv::division {

/**
    executes the platform's native <2N>/<N> division instruction

    \pre upper half of dividend must be strictly less than divisor

    Violating the precondition causes a hardware trap; #DE on x86. Callers are responsible for decomposing inputs that
    don't satisfy this precondition.
*/
template <unsigned_integral dividend_t, unsigned_integral divisor_t = dividend_t> struct instruction_t;

/// generic division; uses compiler's existing division operator
template <unsigned_integral dividend_t, unsigned_integral divisor_t> struct instruction_t
{
    constexpr auto operator()(dividend_t dividend, divisor_t divisor) const noexcept -> div_result_t<divisor_t>
    {
        return {.quotient  = static_cast<divisor_t>(dividend / divisor),
                .remainder = static_cast<divisor_t>(dividend % divisor)};
    }
};

#if defined __x86_64__

/// x64-specific implementation; uses inline asm to execute u128/u64 division instruction directly
template <> struct instruction_t<uint128_t, uint64_t>
{
    auto operator()(uint128_t dividend, uint64_t divisor) const noexcept -> div_result_t<uint64_t>
    {
        assert((dividend >> 64) < divisor && "division parameters will trap");

        auto const high = static_cast<uint64_t>(dividend >> 64);
        auto const low  = static_cast<uint64_t>(dividend);

        div_u128_u64_t result;
        asm volatile("divq %[divisor]"
                     : "=a"(result.quotient), "=d"(result.remainder)
                     : "d"(high), "a"(low), [divisor] "rm"(divisor)
                     : "cc");

        return result;
    }
};

#endif

} // namespace crv::division
