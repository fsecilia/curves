// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel fixed point module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cmath>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

extern "C" {
#include <curves/driver/fixed.h>
}  // extern "C"

#pragma GCC diagnostic pop

namespace curves {

inline auto curves_fixed_from_double(unsigned int frac_bits,
                                     double value) noexcept -> s64 {
  auto scaled_double =
      static_cast<double>(value) * static_cast<double>(1ll << frac_bits);
  auto fixed = static_cast<s64>(scaled_double);
  return fixed;
}

inline auto curves_fixed_to_double(unsigned int frac_bits, s64 value) noexcept
    -> double {
  return static_cast<double>(value) / static_cast<double>(1ll << frac_bits);
}

}  // namespace curves
