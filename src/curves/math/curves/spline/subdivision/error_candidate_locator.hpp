// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Locates maximal error candidate parameters for adaptive subdivision.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/static_vector.hpp>
#include <cassert>
#include <ostream>

namespace curves {

/*!
  Candidate locations to check for maximum error in a cubic segment.

  This type contains candidate locations that are algorithmically determined to
  contain the maximum error across the segment. These are places to check, not
  the error values themselves.
*/
template <typename Scalar>
struct ErrorCandidates {
  // Number of possible locations.
  static const auto max_candidates = 3;

  // Locations to check.
  std::array<Scalar, max_candidates> candidates;

  // Number of locations that were found.
  int count = 0;

  friend auto operator<<(std::ostream& out, const ErrorCandidates& src)
      -> std::ostream& {
    out << "{.t_values = {" << src.candidates[0];
    for (auto location = 1; location < max_candidates; ++location) {
      out << ", " << src.candidates[location];
    }
    out << "}, .count = " << src.count << "}";

    return out;
  }
};

/*!
  Finds locations in a cubic segment to check for maximum error.

  This type is a compile-time strategy to find the most likely locations of
  maximum approximation error in a cubic segment.

  When approximating a smooth curve, this error tends to be in 1 of 3 places.
  Two of these are where segment's tangent is parallel to its secant. The other
  is the inflection point where the curvature is 0.

  It's not perfect because it's comparing against the linear approximation, but
  checking these places causes subdivision to converge more quickly than
  spliting by halves.
*/
template <typename ScalarType>
struct ErrorCandidateLocator {
  using Scalar = ScalarType;

  static constexpr auto max_candidates = 3;
  using Result = StaticVector<Scalar, max_candidates>;

  /*!
    Applies the first derivative test to the deviation function (zeroth-order)
    and its derivative (first-order) to locate error extrema.
  */
  auto operator()(const cubic::Monomial<Scalar>& p) const noexcept -> Result {
    using namespace curves::math;

    Result result;

    // Alias the cubic coefficients we use.
    const auto a = p.coeffs[0];
    const auto b = p.coeffs[1];

    /*
      Candidates 1 & 2: Zeroth-order error extrema.
      This is where the curve's tangent is parallel to the secant.

      The cubic polynomial segment P is parallel to the secant line L when
      their tangents match:

          L(t) = t(P(1) - P(0)) = t(a + b + c)
          L'(t) = a + b + c
          P(t) = at^3 + bt^2 + ct + d
          P'(t) = 3at^2 + 2bt + c

                        P'(t) = L'(t)
              3at^2 + 2bt + c = a + b + c
          3at^2 + 2bt - a - b = 0

      This is nominally a quadratic, but the cubic may be degenerate. Take the
      derivative and see if we still have a quadratic.
    */
    const auto qa = 3 * a;
    const auto qb = 2 * b;
    const auto qc = -(a + b);
    const auto is_quadratic = abs(qa) > 1e-7f;
    if (is_quadratic) {
      // Use the quadratic formula to get the two parallel locations.
      const auto discriminant = qb * qb - 4 * qa * qc;

      // The discriminant is guaranteed to be at least 3*a^2, so there are
      // always 2 locations.
      assert(discriminant > 0.0);
      const auto sqrt_d = sqrt(discriminant);
      const auto t1 = (-qb - sqrt_d) / (2 * qa);
      const auto t2 = (-qb + sqrt_d) / (2 * qa);

      // Only include locations within the segment.
      if (0.0 < t1 && t1 < 1.0) result.push_back(t1);
      if (0.0 < t2 && t2 < 1.0) result.push_back(t2);
    } else {
      // The derivative is not quadratic. See if it's linear.
      const auto is_linear = abs(qb) > 1e-7f;
      if (is_linear) {
        // One parallel location exists.
        const auto t = -qc / qb;
        if (0.0 < t && t < 1.0) result.push_back(t);
      }
    }

    /*
      Candidate 3: First-order error extremum.
      This is the inflection point where the derivative deviation is maximal.

          P''(t) = 6at + 2b = 0
                          t = -b / 3a

      This is the vertex of P'(t), the derivative parabola.
    */
    const auto has_inflection_point = abs(a) > 1e-7f;
    if (has_inflection_point) {
      const auto t_inflection = -b / (3 * a);
      if (0.0 < t_inflection && t_inflection < 1.0) {
        result.push_back(t_inflection);
      }
    }

    return result;
  }
};

}  // namespace curves
