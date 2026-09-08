// SPDX-License-Identifier: MIT

/// \file
/// \brief spline construction errors
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>

namespace crv::spline {

enum class spline_construction_error_reason_t
{
    segment_budget_exhausted,
    minimum_interval_width,
    gain_anchor_not_representable,
    left_endpoint_derivative_not_representable,
};

template <is_fixed t_x_t> struct spline_construction_error_t
{
    using x_t = t_x_t;

    spline_construction_error_reason_t reason;
    x_t left;
    x_t right;

    constexpr auto operator==(spline_construction_error_t const&) const noexcept -> bool = default;
};

} // namespace crv::spline
