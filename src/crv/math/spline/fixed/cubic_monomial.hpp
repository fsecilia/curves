// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic polynomial in monomial form
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/shifter.hpp>
#include <array>

namespace crv::spline::fixed_point {

/// fixed-point cubic polynomial in monomial form with relative shifts between coefficients to align radices
template <is_fixed t_out_t, is_fixed t_in_t, signed_integral t_coeff_t, typename shifter_t = shifter_t<>>
struct cubic_monomial_t
{
    using out_t = t_out_t;
    using in_t = t_in_t;
    using coeff_t = t_coeff_t;

    static constexpr auto coeff_count = 4;

    std::array<coeff_t, coeff_count> coeffs;
    std::array<int_t, coeff_count - 1> shifts;
    int final_frac_bits;

    [[no_unique_address]] shifter_t shifter = shifter_t{};

    [[nodiscard]] constexpr auto operator()(in_t t) const noexcept -> out_t
    {
        using wide_t = widened_t<coeff_t>;

        auto result = coeffs[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff)
        {
            auto const product = wide_t{result} * t.value;
            auto const sum = shifter.shr(product, in_t::frac_bits + shifts[coeff - 1]) + coeffs[coeff];
            result = int_cast<coeff_t>(sum);
        }

        return out_t::literal(
            int_cast<typename out_t::value_t>(shifter.shift(result, out_t::frac_bits - final_frac_bits)));
    }
};

} // namespace crv::spline::fixed_point
