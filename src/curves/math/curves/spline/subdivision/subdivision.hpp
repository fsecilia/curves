// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Definitions common to subdivision.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/math/jet.hpp>
#include <ostream>
#include <vector>

namespace curves {

template <typename Scalar>
struct Knot {
  Scalar v;
  math::Jet<Scalar> y;

  friend auto operator<<(std::ostream& out, const Knot& src) -> std::ostream& {
    return out << "Knot{.v = " << src.v << ", y = " << src.y << "}";
  }
};

template <typename Scalar>
using Knots = std::vector<Knot<Scalar>>;

}  // namespace curves
