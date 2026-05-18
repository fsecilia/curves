// SPDX-License-Identifier: MIT

/// \file
/// \brief L-infinity norm for scalars used to measure error in position and tangent
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/scalar_traits.hpp>
#include <cassert>
#include <cmath>

namespace crv::spline::error_norms {

/// absolute primal error
struct absolute_t
{
    template <typename scalar_t>
    static constexpr auto operator()(scalar_t target, scalar_t approximation) noexcept -> scalar_t
    {
        using std::isfinite;

        auto const result = abs(target - approximation);
        assert(isfinite(result));

        return result;
    }
};

} // namespace crv::spline::error_norms
