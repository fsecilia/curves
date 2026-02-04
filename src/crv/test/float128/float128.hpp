// SPDX-License-Identifier: MIT
/*!
    \file
    \brief 128-bit floating point support

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#if defined CRV_FEATURE_FLOAT_128

#include <quadmath.h>
#include <stdfloat>

#if defined CRV_POLYFILL_FLOAT128_OSTREAM_INSERTER
#include <ostream>
#endif

#if !defined __STDCPP_FLOAT128_T__

using float128_t = __float128;

namespace std {

using ::float128_t;

#if defined CRV_POLYFILL_FLOAT128_EXP2
inline auto exp2(float128_t src) -> float128_t
{
    return exp2q(src);
}
#endif

#if defined CRV_POLYFILL_FLOAT128_LDEXP
inline auto ldexp(float128_t src, int exponent) -> float128_t
{
    return ldexpq(src, exponent);
}
#endif

#if defined CRV_POLYFILL_FLOAT128_LOG2
inline auto log2(float128_t src) -> float128_t
{
    return log2q(src);
}
#endif

#if defined CRV_POLYFILL_FLOAT128_ROUND
inline auto round(float128_t src) -> float128_t
{
    return roundq(src);
}
#endif

#if defined CRV_POLYFILL_FLOAT128_SQRT
inline auto sqrt(float128_t src) -> float128_t
{
    return sqrtq(src);
}
#endif

} // namespace std

#endif // #if !defined __STDCPP_FLOAT128_T__

namespace std {

#if defined CRV_POLYFILL_FLOAT128_OSTREAM_INSERTER
auto operator<<(std::ostream& out, float128_t src) -> std::ostream&;
#endif

} // namespace std

namespace crv {

using std::float128_t;

} // namespace crv

#endif
