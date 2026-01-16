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
#include <cstdint>
#include <ostream>
#include <vector>

namespace curves {

//! Strongly typed node id to prevent arbitrary indexing.
enum class SegmentIndex : uint16_t { Null = 0xFFFF };

struct Knot {
  real_t v;
  math::Jet<real_t> y;

  friend auto operator<<(std::ostream& out, const Knot& src) -> std::ostream& {
    return out << "Knot{.v = " << src.v << ", y = " << src.y << "}";
  }
};

using Knots = std::vector<Knot>;

}  // namespace curves
