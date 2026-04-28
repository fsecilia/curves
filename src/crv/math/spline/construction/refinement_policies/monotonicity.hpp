// SPDX-License-Identifier: MIT

/// \file
/// \brief monotonicity check
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>

namespace crv::spline::refinement_policies {

/// predicate returning true if approximant is monotonic increasing over an interval
struct monotonicity_t
{
    template <is_fixed coeff_t> constexpr auto operator()(coeff_t a, coeff_t b, coeff_t c) const noexcept -> bool
    {
        // For a cubic monomial `p(t) = At^3 + Bt^2 + Ct + D`, the derivative is a parabola `p'(t) = 3At^2 + 2Bt + C`
        // To guarantee monotonicity of `p(t)` on `t` in `[0, 1]`, `p'(t) >= 0` must be true on that interval.
        // Check both endpoints. If the parabola opens upward, check if the vertex, `v_t = -b/3a`, is in `(0, 1)`, and
        // `p'(v_t) < 0`

        using wide_t = fixed::product_t<coeff_t, coeff_t>;

        // check the left endpoint: `p'(0) = C >= 0`
        if (c < coeff_t{0}) return false;

        // check the right endpoint: `p'(1) = 3A + 2B + C >= 0`
        if (multiply(coeff_t{3}, a) + multiply(coeff_t{2}, b) + wide_t::convert(c) < wide_t{0}) return false;

        // if `a <= 0`, the parabola opens downward or is linear, and the minimum must already be at an endpoint
        if (a <= coeff_t{0}) return true;

        // the parabola opens upward, so check if `v_t` is in `(0, 1)`; since `3a > 0`, `0 < -b/3a < 1 == -3a < b < 0`
        if (wide_t::convert(b) <= multiply(coeff_t{-3}, a) || coeff_t{0} <= b) return true;

        // `v_t` is inside `(0, 1)`, so check if `p'(v_t) >= 0`; `c - b^2/3a >= 0 == 3ac >= b^2`
        auto const wide_3ac = 3 * multiply(a, c);
        auto const wide_bb = multiply(b, b);
        return wide_3ac >= wide_bb;
    }
};

} // namespace crv::spline::refinement_policies
