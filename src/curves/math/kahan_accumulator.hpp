// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Kahan summation to compensate for precision loss during addition.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>

namespace curves {

/*!
  Accumulates a sum using Kahan summation.

  Kahan summation tracks the error from each addition, then reintroduces it in
  the next. This compensation increases the accuracy of the sum overall.

  This is a quick, lightweight implementation meant to be a drop-in replacement
  for simple sums consisting soley of operator +=, then reading the final value.
*/
template <typename Value>
struct KahanAccumulator {
  Value sum = Value{0};
  Value compensation = Value{0};

  auto operator+=(Value value) -> void {
    const auto y = value - compensation;
    const auto t = sum + y;
    compensation = (t - sum) - y;
    sum = t;
  }

  operator Value() const { return sum; }
};

}  // namespace curves
