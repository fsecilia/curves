// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Fixed-point input and output.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/driver/math64.h>
#include <array>
#include <ostream>

namespace curves {

inline auto operator<<(std::ostream& out, u128 src) -> std::ostream& {
  if (src == 0) return out << "0";

  std::array<char, 39> buffer;
  auto cur = std::end(buffer);

  while (src > 0) {
    *--cur = '0' + (src % 10);
    src /= 10;
  }

  out.write(cur, std::end(buffer) - cur);

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
