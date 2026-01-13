// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Estimates segment error from a set of candidates.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/math/jet.hpp>
#include <ostream>

namespace curves {

template <typename Scalar>
struct Knot {
  Scalar v;
  math::Jet<Scalar> y;

  friend auto operator<<(std::ostream& out, const Knot& src) -> std::ostream& {
    return out << "Knot{.v = " << src.v << ", y = " << src.y << "}";
  }
};

}  // namespace curves
