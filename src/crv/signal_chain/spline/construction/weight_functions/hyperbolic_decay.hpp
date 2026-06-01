// SPDX-License-Identifier: MIT

/// \file
/// \brief perceptual domain error weight based on hyperbolic decay
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline::weight_functions {

/// weights error based on hyperbolic decay using 1/(t + a)
template <typename scalar_t> struct hyperbolic_decay_t
{
    scalar_t halflife;

    constexpr auto operator()(scalar_t node) const noexcept -> scalar_t { return scalar_t{1.0} / (node + halflife); }
};

} // namespace crv::spline::weight_functions
