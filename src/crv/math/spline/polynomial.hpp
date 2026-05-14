// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic polynomial
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>

namespace crv::spline {

template <typename t_real_t, int_t t_degree = 3> struct polynomial_t : std::array<t_real_t, t_degree + 1>
{
    using real_t = t_real_t;

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

template <typename real_t, typename... remaining_t>
polynomial_t(real_t, remaining_t...)
    -> polynomial_t<std::enable_if_t<(std::same_as<real_t, remaining_t> && ...), real_t>, sizeof...(remaining_t)>;

} // namespace crv::spline
