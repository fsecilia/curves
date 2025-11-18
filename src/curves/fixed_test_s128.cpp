// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia

  This file contains tests for s128 versions of functions that also have an s128
  version.
*/

#include "fixed.hpp"
#include <curves/test.hpp>
#include <algorithm>

#define S128_MAX CURVES_S128_MAX
#define S128_MIN CURVES_S128_MIN

namespace curves {
namespace {

auto operator<<(std::ostream& out, s128 src) -> std::ostream& {
  if (src == 0) return out << "0";

  std::array<char, 39> buffer;
  auto next = std::begin(buffer);

  u128 magnitude = src < 0 ? -static_cast<u128>(src) : static_cast<u128>(src);
  while (magnitude > 0) {
    *next++ = '0' + (magnitude % 10);
    magnitude /= 10;
  }

  std::reverse(buffer.begin(), next);
  out.write(buffer.data(), next - buffer.begin());

  return out;
}

}  // namespace
}  // namespace curves
