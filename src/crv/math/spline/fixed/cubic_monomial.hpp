// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic polynomial in monomial form
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/shifter.hpp>
#include <array>

namespace crv::spline::fixed_point {

/// fixed-point cubic polynomial in monomial form with relative shifts between coefficients to align radices
template <signed_integral t_value_t, typename shifter_t = shifter_t<>> struct cubic_monomial_t
{
    using value_t = t_value_t;

    static constexpr auto normalized_frac_bits = static_cast<int>(sizeof(value_t) * CHAR_BIT);
    using normalized_t                         = fixed_t<make_unsigned_t<value_t>, normalized_frac_bits>;

    static constexpr auto coeff_count = 4;

    std::array<value_t, coeff_count>   coeffs;
    std::array<int_t, coeff_count - 1> shifts;
    int                                final_frac_bits;

    [[no_unique_address]] shifter_t shifter = shifter_t{};

    template <int out_frac_bits>
    [[nodiscard]] constexpr auto evaluate(normalized_t t) const noexcept -> fixed_t<value_t, out_frac_bits>
    {
        using wide_t = widened_t<value_t>;

        auto result = coeffs[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff)
        {
            auto const product = wide_t{result} * t.value;
            auto const sum     = shifter.shr(product, normalized_frac_bits + shifts[coeff - 1]) + coeffs[coeff];
            result             = int_cast<value_t>(sum);
        }

        return fixed_t<value_t, out_frac_bits>::literal(shifter.shift(result, out_frac_bits - final_frac_bits));
    }
};

} // namespace crv::spline::fixed_point
