// SPDX-License-Identifier: MIT

/// \file
/// \brief explicit transfer sampling used by Hermite construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <cmath>
#include <concepts>

namespace crv::spline {

template <typename y_t> struct function_sample_t;

template <std::floating_point t_y_t> struct function_sample_t<t_y_t>
{
    using y_t = t_y_t;
    using scalar_t = y_t;

    scalar_t x;
    y_t y;

    auto operator==(function_sample_t const& src) const noexcept -> bool = default;
};

template <std::floating_point t_scalar_t> struct function_sample_t<jet_t<t_scalar_t>>
{
    using scalar_t = t_scalar_t;
    using y_t = jet_t<scalar_t>;

    scalar_t x;
    y_t y;

    auto operator==(function_sample_t const& src) const noexcept -> bool = default;
};

/// samples the transfer view explicitly for Hermite knot construction
template <typename target_t, std::floating_point scalar_t>
constexpr auto sample_transfer(target_t const& target, scalar_t x) noexcept -> function_sample_t<scalar_t>
{
    auto const result = function_sample_t<scalar_t>{.x = x, .y = target.transfer(x)};

    using std::isfinite;
    assert(isfinite(x));
    assert(isfinite(result.y));
    return result;
}

template <typename target_t, std::floating_point scalar_t>
constexpr auto sample_transfer(target_t const& target, jet_t<scalar_t> x) noexcept -> function_sample_t<jet_t<scalar_t>>
{
    auto const result = function_sample_t<jet_t<scalar_t>>{.x = x.f, .y = target.transfer(x)};

    using std::isfinite;
    assert(isfinite(result.x));
    assert(isfinite(result.y));
    return result;
}

} // namespace crv::spline
