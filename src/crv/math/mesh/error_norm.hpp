// SPDX-License-Identifier: MIT

/// \file
/// \brief L-infinity norms for scalars and 1-jets used to measure error in position and derivative
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/scalar_traits.hpp>
#include <algorithm>

namespace crv::error_norms {

/// standard L-infinity uniform norm for scalars evaluating position error
template <typename scalar_t> struct uniform_t
{
    static constexpr auto operator()(scalar_t target, scalar_t approximation) noexcept -> scalar_t
    {
        using crv::abs;
        return abs(primal(target) - primal(approximation));
    }
};

/// first-order uniform norm for 1-jets evaluating position and derivative error
template <typename jet_t> struct first_order_uniform_t
{
    using scalar_t = jet_t::value_t;

    scalar_t derivative_weight{1.0};

    constexpr auto operator()(jet_t target, jet_t approximation) const noexcept -> scalar_t
    {
        using crv::abs;
        using std::max;

        auto const primal_error = abs(primal(target) - primal(approximation));
        auto const derivative_error = abs(derivative(target) - derivative(approximation)) * derivative_weight;
        return max(primal_error, derivative_error);
    }
};

} // namespace crv::error_norms
