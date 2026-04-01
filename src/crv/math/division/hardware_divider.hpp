// SPDX-License-Identifier: MIT

/// \file
/// \brief native platform division
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/qr_pair.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <cassert>

namespace crv::division {

/// executes the platform's native division instruction
///
/// \pre upper half of dividend must be strictly less than divisor
///
/// Violating the precondition causes a hardware trap; #DE on x64. Callers are responsible for decomposing inputs that
/// don't satisfy this precondition.
template <unsigned_integral t_narrow_t> struct hardware_divider_t
{
    using narrow_t = t_narrow_t;
    using wide_t   = widened_t<narrow_t>;

    /// generic division; uses compiler's existing division operator
    constexpr auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> qr_pair_t<narrow_t>
    {
        return {.quotient  = static_cast<narrow_t>(dividend / divisor),
                .remainder = static_cast<narrow_t>(dividend % divisor)};
    }
};

#if defined __x86_64__

/// uses inline asm to execute u64/u32 division instruction directly
///
/// x64-specific implementation
template <> struct hardware_divider_t<uint32_t>
{
    using wide_t   = uint64_t;
    using narrow_t = uint32_t;

    auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> qr_pair_t<narrow_t>
    {
        auto const high = int_cast<narrow_t>(dividend >> 32);
        auto const low  = int_cast<narrow_t>(dividend & 0xFFFFFFFF);

        assert(high < divisor && "division parameters will trap");

        narrow_t quotient;
        narrow_t remainder;
        asm("divl %[divisor]" : "=a"(quotient), "=d"(remainder) : "d"(high), "a"(low), [divisor] "r"(divisor) : "cc");

        return {quotient, remainder};
    }
};

/// uses inline asm to execute u128/u64 division instruction directly
///
/// x64-specific implementation
template <> struct hardware_divider_t<uint64_t>
{
    using wide_t   = uint128_t;
    using narrow_t = uint64_t;

    auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> qr_pair_t<narrow_t>
    {
        auto const high = int_cast<narrow_t>(dividend >> 64);
        auto const low  = int_cast<narrow_t>(dividend & 0xFFFFFFFFFFFFFFFFull);

        assert(high < divisor && "division parameters will trap");

        narrow_t quotient;
        narrow_t remainder;
        asm("divq %[divisor]" : "=a"(quotient), "=d"(remainder) : "d"(high), "a"(low), [divisor] "r"(divisor) : "cc");

        return {quotient, remainder};
    }
};

#endif

} // namespace crv::division
