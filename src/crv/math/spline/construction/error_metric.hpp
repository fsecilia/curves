// SPDX-License-Identifier: MIT

/// \file
/// \brief metric used to measure error between target function and its approximant
/// \copyright Copyright (C) 2026 Frank Secilia

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
