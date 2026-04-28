// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive spline interval
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <compare>

namespace crv::spline {

/// represents single evaluated interval in an adaptive spline
///
/// Stores domain boundaries, approximant, and max error measured across the segment.
template <typename real_t, typename approximant_t> struct interval_t
{
    real_t left;
    real_t right;
    real_t max_error;
    real_t tolerance;
    int_t depth;
    approximant_t approximant;

    constexpr auto operator<=>(interval_t const& src) const noexcept -> auto = default;
    constexpr auto operator==(interval_t const& src) const noexcept -> bool = default;
};

/// orders intervals by fit priority
struct interval_pred_t
{
    constexpr auto operator()(auto const& lhs, auto const& rhs) const noexcept -> bool
    {
        if (auto const cmp = lhs.max_error <=> rhs.max_error; std::is_neq(cmp)) return std::is_lt(cmp);
        return lhs.left < rhs.left;
    }
};

} // namespace crv::spline
