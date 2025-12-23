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

/*!
  Results of f(x) and f'(x).

  This will eventually support autodifferentiation using dual numbers, but for
  now, it's just the function and its derivative.
*/
struct Jet {
  real_t f;
  real_t df;
};

template <typename Curve>
concept HasCusp = requires(Curve curve) {
  { curve.cusp_location() } -> std::convertible_to<real_t>;
};

}  // namespace curves
