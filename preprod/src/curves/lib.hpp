// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Main C++ library header.

  This file is included by every file in the C++ library.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <cstdint>

namespace curves {

using int_t = std::intptr_t;
using uint_t = std::uintptr_t;

__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;

using real_t = double;
using wide_real_t = long double;
using ssize_t = std::ptrdiff_t;

}  // namespace curves
