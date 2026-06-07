// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>

namespace crv::spline::seed {

/// calculates maximal dyadic stride from a current position toward a target position
///
/// The spline bisects down from a dyadic max domain extent down to a dyadic minimum segment extent. In order to support
/// sinking knots at arbitrary grid positions, the span between two points must be decomposed into a minimal sequence of
/// valid dyadic segments. This type calculates the size of the next segment in that sequence. It does this by finding
/// the maximum step size that satisfies two constraints:
///
/// - alignment: a step of size 2^k can only be taken if the current position is a multiple of 2^k
/// - fit: the step size must not exceed the distance from the current position to the target position
///
/// Alignment is calculated bitwise. Fit is the largest power of two less than or equal to the remaining distance. The
/// result is the minimum of the both. This way, the domain is traversed by stepping up the bisection tree to the
/// largest valid block, and then down as it converges on the target.
template <is_fixed x_t> struct dyadic_stride_calculator_t
{
    static constexpr auto operator()(x_t const current, x_t const target) noexcept -> x_t
    {
        // crack fixed
        auto const current_value = current.value;
        auto const target_value = target.value;

        // apply bitwise alignment to underlying
        using signed_t = x_t::value_t;
        using unsigned_t = make_unsigned_t<signed_t>;
        auto const max_align_step = (current_value == 0) ? target_value : (current_value & -current_value);
        auto const max_fit_step
            = static_cast<signed_t>(std::bit_floor(static_cast<unsigned_t>(target_value - current_value)));

        // repack fixed
        return x_t::literal(min(max_align_step, max_fit_step));
    }
};

} // namespace crv::spline::seed
