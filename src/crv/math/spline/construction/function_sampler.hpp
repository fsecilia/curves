// SPDX-License-Identifier: MIT

/// \file
/// \brief L-infinity norms for scalars and 1-jets used to measure error in position and tangent
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <concepts>

namespace crv::spline {

/// float x and jet y result of sampling a function at x
template <std::floating_point scalar_t> struct function_sample_t
{
    scalar_t x;
    jet_t<scalar_t> y;

    auto operator==(function_sample_t const& src) const noexcept -> bool = default;
};

// samples target function, returning the sample location and resulting 1-jet
template <typename target_function_t> struct function_sampler_t
{
    target_function_t target_function;

    template <std::floating_point scalar_t>
    constexpr auto operator()(scalar_t x) const noexcept -> function_sample_t<scalar_t>
    {
        auto const result = function_sample_t<scalar_t>{.x = x, .y = target_function(jet_t<scalar_t>{x, 1.0})};

        using std::isfinite;
        assert(isfinite(x));
        assert(isfinite(result.y));

        return result;
    }
};

} // namespace crv::spline
