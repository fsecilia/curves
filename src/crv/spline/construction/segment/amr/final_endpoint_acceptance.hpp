// SPDX-License-Identifier: MIT

/// \file
/// \brief final endpoint acceptance for tangent seeding
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <concepts>

namespace crv::spline {

/// decides whether a final endpoint can seed tangent construction
template <is_fixed y_t, std::floating_point scalar_t> struct final_endpoint_acceptance_t
{
    static constexpr auto operator()(y_t encoded_anchor, scalar_t right_gain_slope, y_t y_limit) noexcept -> bool
    {
        return encoded_anchor >= y_t{0} && encoded_anchor <= y_limit && right_gain_slope >= scalar_t{0};
    }
};

} // namespace crv::spline
