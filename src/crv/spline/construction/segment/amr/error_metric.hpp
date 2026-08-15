// SPDX-License-Identifier: MIT

/// \file
/// \brief metric used to measure error between target function and its approximant
/// \copyright Copyright (C) 2026 Frank Secilia
///
/// During development, many metrics were tested. Simple Sobolev has no units and tuning is arbitrary. Including units
/// in Sobolev requires scaling the derivative term by a factor with units, or canceling units in the linear term. Both
/// require smooth max. Neither produces more accurate curves when accuracy is later measured using L_1.

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <cassert>
#include <cmath>

namespace crv::spline {

/// pointwise L_1 metric
struct error_metric_t
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

} // namespace crv::spline
