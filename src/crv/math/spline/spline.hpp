// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic spline
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <array>
#include <cassert>
#include <cstddef>

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
    constexpr spline_t(
        segment_locator_t const& locator, segments_t const& segments, int_t segment_count, in_t x_max) noexcept
        : x_max_{x_max}, segment_count_{segment_count}, locate_segment_{locator}, segments_{segments}
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

        if !consteval { prev_segment_index_ = location.index; }

        return segments_[location.index](x - location.origin);
    }

    /// validates data the driver receives
    constexpr auto is_valid() const noexcept -> bool
    {
        // segment count must be in [0, max_segments]
        if (segment_count_ <= 0 || max_segments < segment_count_) return false;

        // domain must not be degenerate or empty
        if (x_max_ <= in_t{0}) return false;

        // dispatch to segment locator
        if (!locate_segment_.is_valid(segment_count_)) return false;

        // dispatch to each segment
        for (int_t i = 0; i < segment_count_; ++i)
        {
            if (!segments_[i].is_valid()) return false;
        }

        return true;
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void
    {
        prefetch_segments(prefetcher);
        locate_segment_.prefetch(prefetcher);
    }

private:
    constexpr auto extend_final_tangent(in_t x) const noexcept -> out_t
    {
        return segments_[segment_count_ - 1].extend_final_tangent(x - x_max_);
    }

    /// prefetches the most recently selected segment and the two adjacent
    ///
    /// Prefetching these 3 segments serves as our hint to exploit the natural temporal locality of mouse velocity.
    constexpr auto prefetch_segments(auto const& prefetcher) const noexcept -> void
    {
        // these casts are required to prevent ub when forming addresses outside of the array
        auto const* base = static_cast<std::byte const*>(static_cast<void const*>(segments_.data()));

        // prefetch most recent segment
        //
        // Because segments_ is aligned to a cache line, one of these two will contain the cache line containing the
        // previous segment and one adjacent, and the other will contain the other adjacent. If prev_segment_index is 0
        // or segment_count_ - 1, these both technically are out of bounds, either prefetching the end of the segment
        // locator or whatever follows this type as a whole, but prefetching is built to be resiliant to this pattern.
        // We do not have to guard it with an condition, saving a pair of misprediction sources.
        prefetcher.prefetch(base + ((prev_segment_index_ - 1) * sizeof(segment_t)));
        prefetcher.prefetch(base + ((prev_segment_index_ + 1) * sizeof(segment_t)));
    }

    // these are ordered for overall cache-friendliness in operator ()
    in_t x_max_{};
    int_t segment_count_{};
    segment_locator_t locate_segment_{};

    // *must* be aligned or the prefetching scheme is useless
    alignas(64) segments_t segments_{};

    mutable int_t prev_segment_index_ = 0;
};

} // namespace crv::spline
