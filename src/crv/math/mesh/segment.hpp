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
template <typename real_t, typename approximant_t> struct mesh_segment_t
{
    real_t left;
    real_t right;
    approximant_t approximant;
    real_t max_error;

    auto operator<=>(mesh_segment_t const& src) const noexcept -> auto = default;
    auto operator==(mesh_segment_t const& src) const noexcept -> bool = default;
};

/// ordered mesh segments by max_measured_error
struct mesh_segment_pred_t
{
    constexpr auto operator()(auto const& left, auto const& right) const noexcept -> bool
    {
        return left.max_error < right.max_error;
    }
};

} // namespace crv
