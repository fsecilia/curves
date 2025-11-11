// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel fixed point module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cmath>

extern "C" {
#include <curves/driver/fixed.h>
}  // extern "C"

namespace curves {

inline auto curves_fixed_from_double(unsigned int frac_bits,
                                     double value) noexcept -> curves_fixed_t {
  auto scaled_double =
      static_cast<double>(value) * static_cast<double>(1ll << frac_bits);
  auto fixed = static_cast<curves_fixed_t>(scaled_double);
  return fixed;
}

inline auto curves_fixed_to_double(unsigned int frac_bits,
                                   curves_fixed_t value) noexcept -> double {
  return static_cast<double>(value) / static_cast<double>(1ll << frac_bits);
}

}  // namespace curves
