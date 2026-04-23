// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic spline
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <array>
#include <cassert>
#include <utility>

namespace crv::spline {

/// fixed-point cubic spline approximating a function over a specific domain
template <typename segment_t, typename segment_locator_t> class spline_t
{
public:
    using in_t = segment_t::in_t;
    using out_t = segment_t::out_t;

    static constexpr auto max_segments = 256;
    using segments_t = std::array<segment_t, max_segments>;

    /// \pre segment_count > 0
    constexpr spline_t(segment_locator_t locator, segments_t segments, int_t segment_count, in_t x_max) noexcept
        : x_max_{x_max}, segment_count_{segment_count}, locate_segment_{std::move(locator)},
          segments_{std::move(segments)}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<spline_t>);

        assert(segment_count_ && "spline_t: requires nonzero segment count");
        assert(segment_count_ <= max_segments && "spline_t: segment count exceeds capacity");
    }

    constexpr auto operator()(in_t x) const noexcept -> out_t
    {
        if (x >= x_max_) return extend_final_tangent(x);

        auto const location = locate_segment_(x);
        assert((!is_signed_v<decltype(location)> || location.index >= 0) && location.index < segment_count_
            && "spline_t: located segment index out of bounds");

        assert(location.origin <= x && "spline_t: located segment origin underflows");
        return segments_[location.index](x - location.origin);
    }

private:
    constexpr auto extend_final_tangent(in_t x) const noexcept -> out_t
    {
        return segments_[segment_count_ - 1].extend_final_tangent(x - x_max_);
    }

    // these are ordered for overall cache-friendliness in operator ()
    in_t x_max_{};
    int_t segment_count_{};
    segment_locator_t locate_segment_{};
    segments_t segments_{};
};

} // namespace crv::spline
