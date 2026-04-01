// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive mesh interval
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/mesh/measured_error.hpp>

namespace crv {

/// represents single evaluated interval in an adaptive mesh
///
/// Stores domain boundaries, payload, and max error measured across the segment.
template <typename real_t, typename payload_t> struct mesh_segment_t
{
    real_t                   left;
    real_t                   right;
    payload_t                payload;
    measured_error_t<real_t> max_measured_error;

    auto operator<=>(mesh_segment_t const& src) const noexcept -> auto = default;
    auto operator==(mesh_segment_t const& src) const noexcept -> bool  = default;
};

/// ordered mesh segments by max_measured_error
template <typename measured_error_pred_t> struct mesh_segment_pred_t
{
    measured_error_pred_t measured_error_pred;

    constexpr auto operator()(auto const& left, auto const& right) const noexcept -> bool
    {
        return measured_error_pred(left.max_measured_error, right.max_measured_error);
    }
};

} // namespace crv
