// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive subdivision loop
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/quadrature/bisector.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <algorithm>

namespace crv::quadrature {
namespace generic {

/// decides whether a segment should be subdivided
template <typename real_t> struct subdivision_predicate_t
{
    using segment_t = segment_t<real_t>;

    static constexpr auto min_width             = epsilon<real_t>() * real_t{1024};
    static constexpr auto relative_noise_margin = epsilon<real_t>() * real_t{64};

    constexpr auto operator()(segment_t const& segment, real_t area, real_t error, int_t depth_limit) const noexcept
        -> bool
    {
        auto const current_width   = segment.right - segment.left;
        auto const noise_floor     = abs(area) * relative_noise_margin;
        auto const local_tolerance = std::max(segment.tolerance, noise_floor);
        return segment.depth < depth_limit && current_width > min_width && error > local_tolerance;
    }
};

} // namespace generic
} // namespace crv::quadrature
