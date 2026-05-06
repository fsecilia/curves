// SPDX-License-Identifier: MIT

/// \file
/// \brief L-infinity norms for scalars and 1-jets used to measure error in position and tangent
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/scalar_traits.hpp>
#include <cassert>
#include <cmath>

namespace crv::spline::error_norms {

/// soft floor using lse
template <typename real_t> struct logsumexp_floor_t
{
    static constexpr auto sharpness = real_t{10};

    static constexpr auto operator()(real_t x, real_t y) noexcept -> real_t
    {
        using std::exp;
        using std::isfinite;
        using std::log;

        // cache division result
        static constexpr auto rsharpness = 1.0 / sharpness;

        // stability transform
        //
        // The standard function is:
        //
        //     lse_floor(x, y) = log(exp(kx) + exp(ky))/k
        //
        // However, this may overflow because exponentials grow quickly. Instead, guarantee the exponent is always 0 or
        // negative:
        //
        //     m = max(x, y)
        //     lse_floor_stable(x, y) = m + log(exp(k(m - x) + exp(k(m - y)))/y
        //
        auto const m = max(x, y);
        auto const result = m + log(exp(sharpness * (x - m)) + exp(sharpness * (y - m))) * rsharpness;
        assert(isfinite(result));

        return result;
    }
};

/// standard L-infinity uniform norm for scalars evaluating position error
template <typename scalar_t> struct uniform_t
{
    static constexpr auto operator()(scalar_t target, scalar_t approximation) noexcept -> scalar_t
    {
        using crv::abs;
        return abs(primal(target) - primal(approximation));
    }
};

/// first-order uniform norm for 1-jets evaluating position and tangent error
template <typename jet_t> struct first_order_uniform_t
{
    using scalar_t = jet_t::value_t;

    scalar_t tangent_weight{1.0};

    constexpr auto operator()(jet_t target, jet_t approximation) const noexcept -> scalar_t
    {
        using crv::abs;
        using crv::max;

        auto const primal_error = abs(primal(target) - primal(approximation));
        auto const tangent_error = abs(tangent(target) - tangent(approximation)) * tangent_weight;
        return max(primal_error, tangent_error);
    }
};

} // namespace crv::spline::error_norms
