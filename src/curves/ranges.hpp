// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Safe numerical casts.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <concepts>
#include <ranges>

namespace curves {

template <typename Range>
concept ScalarRange = std::ranges::range<Range> &&
                      std::floating_point<std::ranges::range_value_t<Range>>;

}  // namespace curves
