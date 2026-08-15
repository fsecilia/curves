// SPDX-License-Identifier: MIT

/// \file
/// \brief polynomial coordinate conversion from normalized Hermite t to local runtime u
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/math/polynomial.hpp>
#include <cassert>
#include <concepts>

namespace crv::spline {

/// reparameterizes a normalized cubic from t in [0, 1] to local coordinate u = x - x0
///
/// Given t = u / width, this converts
///     c3*t^3 + c2*t^2 + c1*t + c0
/// into
///     (c3/width^3) u^3 + (c2/width^2) u^2 + (c1/width) u + c0.
template <std::floating_point scalar_t> struct local_coordinate_converter_t
{
    constexpr auto operator()(cubic_t<scalar_t> const& normalized, scalar_t width) const noexcept -> cubic_t<scalar_t>
    {
        assert(width > scalar_t{0});

        auto const inverse_width = scalar_t{1} / width;
        auto const inverse_width_squared = inverse_width * inverse_width;

        return {
            normalized[0] * inverse_width_squared * inverse_width,
            normalized[1] * inverse_width_squared,
            normalized[2] * inverse_width,
            normalized[3],
        };
    }
};

} // namespace crv::spline
