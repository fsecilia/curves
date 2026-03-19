// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rounding_divider.hpp"
#include <crv/math/division/result.hpp>
#include <crv/test/test.hpp>

namespace crv::division {
namespace {

using narrow_t = uint32_t;
using wide_t   = wider_t<narrow_t>;
using result_t = result_t<wide_t, narrow_t>;

struct rounding_mode_t
{
    constexpr auto div_bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t
    {
        return dividend * 2 + divisor;
    }

    constexpr auto div_carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t
    {
        return quotient * 7 + divisor * 5 + remainder * 3;
    }
};

struct divider_t
{
    constexpr auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t
    {
        return {dividend * 13, divisor * 11};
    }
};

// arbitrary test inputs
constexpr auto dividend = wide_t{17};
constexpr auto divisor  = narrow_t{23};

// expected flow through fakes
constexpr auto expected_biased_dividend = dividend * 2 + divisor;
constexpr auto expected_quotient        = expected_biased_dividend * 13;
constexpr auto expected_remainder       = divisor * 11;

// expected final output
constexpr auto expected_final_result = expected_quotient * 7 + divisor * 5 + expected_remainder * 3;

static_assert(rounding_divider_t<narrow_t, divider_t>{}(dividend, divisor, rounding_mode_t{}) == expected_final_result);

} // namespace
} // namespace crv::division
