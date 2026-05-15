// SPDX-License-Identifier: MIT

/// \file
/// \brief polynomial in monomial basis
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// polynomial_t
// --------------------------------------------------------------------------------------------------------------------

template <typename t_scalar_t, int_t t_degree> struct polynomial_t : std::array<t_scalar_t, t_degree + 1>
{
    using scalar_t = t_scalar_t;

    static constexpr auto degree = t_degree;
    static constexpr auto term_count = degree + 1;

    template <typename value_t> constexpr auto operator()(value_t t) const noexcept -> value_t
    {
        auto const coeffs = this->data();
        auto const coeff_count = this->size();
        auto accumulator = static_cast<value_t>(coeffs[0]);
        for (auto coeff_index = 1u; coeff_index < coeff_count; ++coeff_index)
        {
            accumulator = accumulator * t + coeffs[coeff_index];
        }
        return accumulator;
    }
};

template <typename scalar_t, typename... remaining_t>
polynomial_t(scalar_t, remaining_t...)
    -> polynomial_t<std::enable_if_t<(std::same_as<scalar_t, remaining_t> && ...), scalar_t>, sizeof...(remaining_t)>;

template <std::floating_point scalar_t> using cubic_t = polynomial_t<scalar_t, 3>;

// --------------------------------------------------------------------------------------------------------------------
// hermite_converter_t
// --------------------------------------------------------------------------------------------------------------------

/// converts hermite basis to monomial basis
template <std::floating_point scalar_t> struct hermite_converter_t
{
    template <typename jet_t> constexpr auto operator()(jet_t left_y, jet_t right_y) const noexcept -> cubic_t<scalar_t>
    {
        auto const p0 = primal(left_y);
        auto const p1 = primal(right_y);
        auto const m0 = tangent(left_y);
        auto const m1 = tangent(right_y);
        auto const dp = p1 - p0;

        auto const a = -2.0 * dp + m0 + m1;
        auto const b = 3.0 * dp - 2.0 * m0 - m1;
        auto const c = m0;
        auto const d = p0;

        return {a, b, c, d};
    }
};

} // namespace crv
