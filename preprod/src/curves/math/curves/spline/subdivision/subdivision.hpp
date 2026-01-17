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
#include <curves/numeric_cast.hpp>
#include <cstdint>
#include <ostream>
#include <vector>

namespace curves {

struct SubdivisionConfig {
  int_t segments_max = 256;
  real_t segment_width_min = 1.0 / (1 << 16);  // 2^-16
  real_t error_tolerance = 1e-6;
};

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

struct Segment {
  Knot start;
  Knot end;
  cubic::Monomial poly;
  real_t max_error;
  real_t v_split;

  auto width() const noexcept -> real_t { return end.v - start.v; }

  auto operator<(const Segment& other) const noexcept -> bool {
    return max_error < other.max_error;
  }
};

/*!
  Output of adaptive subdivision, ready for spline construction.

  Contains parallel arrays of knot positions and segment polynomials.
  knots.size() == polys.size() + 1.
*/
struct QuantizedSpline {
  std::vector<real_t> knots;           // Quantized positions, Q8.24
  std::vector<cubic::Monomial> polys;  // Quantized coefficients

  auto segment_count() const noexcept -> int_t {
    return numeric_cast<int_t>(polys.size());
  }
};

}  // namespace curves
