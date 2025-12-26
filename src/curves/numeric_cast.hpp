// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Safe numerical casts.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cassert>
#include <concepts>
#include <utility>

namespace curves {

template <std::integral To, std::integral From>
constexpr auto numeric_cast(From from) noexcept -> To {
  assert(std::in_range<To>(from) && "out of range numeric cast");
  return static_cast<To>(from);
}

}  // namespace curves
