// SPDX-License-Identifier: MIT

/// \file
/// \brief main user-mode library header
///
/// This file is included by every file in the user-mode library.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
#error "Curves requires floating-point infinities and NaNs; -ffast-math and -ffinite-math-only are unsupported"
#endif

#include <cstdint>

namespace crv {

using int_t = std::intptr_t;
using uint_t = std::uintptr_t;

using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;

using float32_t = float;
using float64_t = double;
using float_max_t = long double;
using float_t = float64_t;

#if defined(__GNUC__) || defined(__clang__)
#define CRV_ALWAYS_INLINE [[gnu::always_inline]]
#else
#define CRV_ALWAYS_INLINE
#endif

} // namespace crv
