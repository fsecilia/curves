// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/shifter.hpp>

namespace crv::spline::fixed_point {

/// fixed-point cubic spline segment anchoring a monomial to position and width
///
/// \pre rwidth > 0
/// \pre rwidth_frac_bits >= 0
template <typename monomial_t, typename t_in_t, typename shifter_t = shifter_t<>> struct cubic_segment_t
{
    using in_t         = t_in_t;
    using normalized_t = monomial_t::in_t;
    using out_t        = monomial_t::out_t;
    using value_t      = in_t::value_t;

    monomial_t monomial;
    value_t    rwidth;
    int        rwidth_frac_bits;

    [[no_unique_address]] shifter_t shifter = shifter_t{};

    // \pre 0 <= dx
    // \pre dx < 1/rwidth
    [[nodiscard]] constexpr auto operator()(in_t dx) const noexcept -> out_t
    {
        using wide_t = widened_t<value_t>;

        assert(rwidth > 0);
        assert(rwidth_frac_bits >= 0);

        assert(0 <= dx.value);
        assert(dx < in_t::literal(static_cast<in_t::value_t>(
                   (static_cast<wide_t>(1) << (rwidth_frac_bits + in_t::frac_bits)) / rwidth)));

        auto const normalizing_shift = normalized_t::frac_bits - in_t::frac_bits - rwidth_frac_bits;
        auto const t                 = normalized_t::literal(
            int_cast<typename normalized_t::value_t>(shifter.shift(wide_t{dx.value} * rwidth, normalizing_shift)));
        return monomial(t);
    }
};

} // namespace crv::spline::fixed_point
