// SPDX-License-Identifier: MIT

/// \file
/// \brief samples a construction target's transfer view, retaining location and result
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <cmath>
#include <concepts>

namespace crv::spline {

template <typename y_t> struct function_sample_t;

/// scalar result of sampling a function
template <std::floating_point t_y_t> struct function_sample_t<t_y_t>
{
    using y_t = t_y_t;

    using scalar_t = y_t;

    scalar_t x;
    y_t y;

    auto operator==(function_sample_t const& src) const noexcept -> bool = default;
};

/// jet result of sampling a function
template <std::floating_point t_scalar_t> struct function_sample_t<jet_t<t_scalar_t>>
{
    using scalar_t = t_scalar_t;
    using y_t = jet_t<scalar_t>;

    scalar_t x;
    y_t y;

    auto operator==(function_sample_t const& src) const noexcept -> bool = default;
};

// samples target.transfer(), returning the sample location and resulting value
//
// The sampler is deliberately transfer-specific. Gain and sensitivity targets expose both mathematical views, while
// the current Hermite/packed-spline path continues to approximate transfer only.
template <typename t_target_t> struct transfer_sampler_t
{
    using target_t = t_target_t;

    target_t target;

    template <std::floating_point scalar_t>
    constexpr auto operator()(scalar_t x) const noexcept -> function_sample_t<scalar_t>
    {
        auto const result = function_sample_t<scalar_t>{.x = x, .y = target.transfer(x)};

        using std::isfinite;
        assert(isfinite(x));
        assert(isfinite(result.y));

        return result;
    }

    template <std::floating_point scalar_t>
    constexpr auto operator()(jet_t<scalar_t> x) const noexcept -> function_sample_t<jet_t<scalar_t>>
    {
        auto const result = function_sample_t<jet_t<scalar_t>>{.x = x.f, .y = target.transfer(x)};

        using std::isfinite;
        assert(isfinite(result.x));
        assert(isfinite(result.y));

        return result;
    }
};

} // namespace crv::spline
