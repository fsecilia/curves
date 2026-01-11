// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Transfer function concepts.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <concepts>

namespace curves {

template <typename Curve>
concept HasAntiderivative =
    requires(const Curve curve, typename Curve::Scalar scalar) {
      {
        curve.antiderivative(scalar)
      } -> std::convertible_to<typename Curve::Scalar>;
    };

}  // namespace curves
