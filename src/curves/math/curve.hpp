// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Base curve definitions.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>

namespace curves {

struct CurveResult {
  real_t f;
  real_t df_dx;
};

}  // namespace curves
