// SPDX-License-Identifier: MIT
/*!
    \file
    \brief 128-bit floating point support

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <quadmath.h>
#include <stdfloat>

#if !defined __STDCPP_FLOAT128_T__ && !defined __FLOAT128__
#error "std::float128_t unavailable"
#endif

#if defined polyfill_float128_ostream_inserter
#include <ostream>
#endif

namespace std {

#if !defined __STDCPP_FLOAT128_T__ && defined __FLOAT128__
using float128_t = __float128;
#endif

#if defined polyfill_float128_ostream_inserter
auto operator<<(std::ostream& out, float128_t src) -> std::ostream&;
#endif

#if defined polyfill_float128_abs
inline constexpr auto abs(float128_t const& src) noexcept -> float128_t
{
    return src < 0 ? -src : src;
}
#endif

#if defined polyfill_float128_ldexp
inline auto ldexp(float128_t const& src, int exponent) -> float128_t
{
    return ldexpq(src, exponent);
}
#endif

} // namespace std

namespace crv {

using float128_t = std::float128_t;

} // namespace crv
