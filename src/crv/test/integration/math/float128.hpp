// SPDX-License-Identifier: MIT
/*!
    \file
    \brief 128-bit floating point support

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <ostream>
#include <stdfloat>

#if !defined __STDCPP_FLOAT128_T__ && !defined __FLOAT128__
#error "std::float128_t unavailable"
#endif

namespace std {

#if !defined __STDCPP_FLOAT128_T__ && defined __FLOAT128__
using float128_t = __float128;
#endif

} // namespace std

namespace crv {

using float128_t = std::float128_t;

auto operator<<(std::ostream& out, float128_t src) -> std::ostream&;

} // namespace crv
