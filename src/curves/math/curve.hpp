// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Composed spline and input shaping for curve evaluation.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/input_shaping.hpp>
#include <curves/math/spline.hpp>

namespace curves {

struct Curve {
  curves_spline spline;
  curves_shaping_params shaping;
};

}  // namespace curves
