// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive mesh interval
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv {

/// represents single evaluated interval in an adaptive mesh
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

/// ordered mesh segments by max_measured_error
struct interval_pred_t
{
    constexpr auto operator()(auto const& lhs, auto const& rhs) const noexcept -> bool
    {
        if (auto const cmp = lhs.max_error <=> rhs.max_error; cmp != 0) return cmp < 0;
        return lhs.left < rhs.left;
    }
};

} // namespace crv
