// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Fixed-point input and output.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/fixed.hpp>
#include <algorithm>
#include <array>
#include <ostream>

namespace curves {

inline auto operator<<(std::ostream& out, u128 src) -> std::ostream& {
  if (src == 0) return out << "0";

  std::array<char, 39> buffer;
  auto next = std::begin(buffer);

  while (src > 0) {
    *next++ = '0' + (src % 10);
    src /= 10;
  }

  std::reverse(buffer.begin(), next);
  out.write(buffer.data(), next - buffer.begin());

  return out;
}

inline auto operator<<(std::ostream& out, s128 src) -> std::ostream& {
  if (src < 0) {
    return out << "-" << -static_cast<u128>(src);
  } else {
    return out << static_cast<u128>(src);
  }
}

}  // namespace curves
