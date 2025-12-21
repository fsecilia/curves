// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Base curve definitions.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <concepts>

namespace curves {

struct CurveResult {
  real_t f;
  real_t df_dx;
};

template <typename Curve>
concept HasCusp = requires(Curve curve) {
  { curve.cusp_location() } -> std::convertible_to<real_t>;
};

}  // namespace curves
