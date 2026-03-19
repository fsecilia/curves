// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "normalizing_dividers.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <compare>
#include <gmock/gmock.h>

namespace crv::division::prescaling_dividers {
namespace {

using narrow_t = uint8_t;
using wide_t   = wider_t<narrow_t>;

// --------------------------------------------------------------------------------------------------------------------
// safe_clz
// --------------------------------------------------------------------------------------------------------------------

static_assert(detail::safe_clz(wide_t{0}) == sizeof(wide_t) * CHAR_BIT - 1);
static_assert(detail::safe_clz(wide_t{1}) == sizeof(wide_t) * CHAR_BIT - 1);
static_assert(detail::safe_clz(wide_t{2}) == sizeof(wide_t) * CHAR_BIT - 2);
static_assert(detail::safe_clz(wide_t{0x7FFF}) == 1);
static_assert(detail::safe_clz(wide_t{0x8000}) == 0);
static_assert(detail::safe_clz(wide_t{0xFFFF}) == 0);

// --------------------------------------------------------------------------------------------------------------------
// Prescaling Dividers Support
// --------------------------------------------------------------------------------------------------------------------

struct rounding_mode_t
{
    constexpr auto use(wide_t dividend, narrow_t divisor) const noexcept -> wide_t { return dividend * 2 + divisor; }

    constexpr auto div_bias(wide_t, narrow_t) const noexcept -> wide_t;
    constexpr auto div_carry(wide_t, narrow_t, narrow_t) const noexcept -> wide_t;
};
constexpr auto rounding_mode = rounding_mode_t{};

struct divider_t
{
    constexpr auto operator()(wide_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept -> wide_t
    {
        return rounding_mode.use(dividend, divisor);
    }
};

auto const dividend    = narrow_t{17};
auto const divisor     = narrow_t{23};
auto const total_shift = 3;

// --------------------------------------------------------------------------------------------------------------------
// variable_t
// --------------------------------------------------------------------------------------------------------------------

// nominal case
static_assert(rounding_mode.use(int_cast<wide_t>(dividend) << total_shift, divisor)
              == variable_t<narrow_t, total_shift, divider_t>{}(dividend, divisor, rounding_mode));

// total_shift = 0: no shift, just widen and forward
static_assert(rounding_mode.use(int_cast<wide_t>(dividend), divisor)
              == variable_t<narrow_t, 0, divider_t>{}(dividend, divisor, rounding_mode));

// dividend = 0: safe_clz returns width - 1, but shift of 0 is still 0
static_assert(rounding_mode.use(int_cast<wide_t>(0), divisor)
              == variable_t<narrow_t, total_shift, divider_t>{}(narrow_t{0}, divisor, rounding_mode));

// --------------------------------------------------------------------------------------------------------------------
// constant_t
// --------------------------------------------------------------------------------------------------------------------

// nominal case
static_assert(rounding_mode.use(int_cast<wide_t>(dividend << total_shift), divisor)
              == constant_t<narrow_t, dividend, total_shift, divider_t>{}(dividend, divisor, rounding_mode));

// total_shift = 0: no shift, just widen and forward
static_assert(rounding_mode.use(int_cast<wide_t>(dividend), divisor)
              == constant_t<narrow_t, dividend, 0, divider_t>{}(dividend, divisor, rounding_mode));

// dividend = 0: safe_clz returns width - 1, but shift of 0 is still 0
static_assert(rounding_mode.use(int_cast<wide_t>(0), divisor)
              == constant_t<narrow_t, 0, total_shift, divider_t>{}(narrow_t{0}, divisor, rounding_mode));

} // namespace
} // namespace crv::division::prescaling_dividers
