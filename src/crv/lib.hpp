// SPDX-License-Identifier: MIT
/*!
  \file
  \brief main user-mode library header

  This file is included by every file in the user-mode library.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <cstdint>

namespace crv {

using int_t  = std::intptr_t;
using uint_t = std::uintptr_t;

__extension__ typedef __int128          int128_t;
__extension__ typedef unsigned __int128 uint128_t;

using real32_t  = float;
using real64_t  = double;
using real_t    = real64_t;
using realmax_t = long double;

} // namespace crv
