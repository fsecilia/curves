// SPDX-License-Identifier: MIT

/// \file
/// \brief native platform division
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/int_traits.hpp>
#include <cassert>

namespace crv::division {

/// executes the platform's native division instruction
///
/// \pre upper half of dividend must be strictly less than divisor
///
/// Violating the precondition causes a hardware trap; #DE on x64. Callers are responsible for decomposing inputs that
/// don't satisfy this precondition.
template <unsigned_integral dividend_t, unsigned_integral divisor_t> struct hardware_divider_t;

/// generic division; uses compiler's existing division operator
template <unsigned_integral narrow_t> struct hardware_divider_t<wider_t<narrow_t>, narrow_t>
{
    using wide_t = wider_t<narrow_t>;

    constexpr auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t<narrow_t>
    {
        return {.quotient  = static_cast<narrow_t>(dividend / divisor),
                .remainder = static_cast<narrow_t>(dividend % divisor)};
    }
};

#if defined __x86_64__

/// x64-specific implementation; uses inline asm to execute u64/u32 division instruction directly
template <> struct hardware_divider_t<uint64_t, uint32_t>
{
    auto operator()(uint64_t dividend, uint32_t divisor) const noexcept -> result_t<uint32_t>
    {
        assert((dividend >> 32) < divisor && "division parameters will trap");

        auto const high = static_cast<uint32_t>(dividend >> 32);
        auto const low  = static_cast<uint32_t>(dividend);

        result_t<uint32_t> result;
        asm("divl %[divisor]"
            : "=a"(result.quotient), "=d"(result.remainder)
            : "d"(high), "a"(low), [divisor] "r"(divisor)
            : "cc");

        return result;
    }
};

/// x64-specific implementation; uses inline asm to execute u128/u64 division instruction directly
template <> struct hardware_divider_t<uint128_t, uint64_t>
{
    auto operator()(uint128_t dividend, uint64_t divisor) const noexcept -> result_t<uint64_t>
    {
        assert((dividend >> 64) < divisor && "division parameters will trap");

        auto const high = static_cast<uint64_t>(dividend >> 64);
        auto const low  = static_cast<uint64_t>(dividend);

        result_t<uint64_t> result;
        asm("divq %[divisor]"
            : "=a"(result.quotient), "=d"(result.remainder)
            : "d"(high), "a"(low), [divisor] "r"(divisor)
            : "cc");

        return result;
    }
};

#endif

} // namespace crv::division
