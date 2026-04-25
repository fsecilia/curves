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
    using x_t = segment_t::x_t;
    using y_t = segment_t::y_t;

    static constexpr auto max_segments = 256;
    using segments_t = std::array<segment_t, max_segments>;

    /// \pre 0 < segment_count <= max_segments
    constexpr spline_t(segment_locator_t const& locator, segments_t const& segments, int_t segment_count) noexcept
        : spline_t{locator, segments, segment_count, 0}
    {
        assert(fields_are_valid() && "spline_t: local fields invalid");
    }

    constexpr spline_t(segment_locator_t const& locator, segments_t const& segments, int_t segment_count,
        int_t prev_segment_index) noexcept
        : segment_count_{segment_count}, segment_locator_{locator}, segments_{segments},
          prev_segment_index_{prev_segment_index}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<spline_t>);
    }

    /// \pre 0 <= x
    constexpr auto operator()(x_t x) const noexcept -> y_t
    {
        assert(x_t{0} <= x && "spline_t: input out of bounds");

        auto const x_max = segment_locator_.x_max();
        if (x >= x_max) return extend_final_tangent(x - x_max);

        auto const location = segment_locator_.locate(x);
        assert(
            0 <= location.index && location.index < segment_count_ && "spline_t: located segment index out of bounds");
        assert(0 <= location.origin && location.origin <= x && "spline_t: located segment origin out of range");

        if !consteval { prev_segment_index_ = location.index; }

        return segments_[location.index].evaluate(x - location.origin);
    }

    /// validates data the driver receives
    constexpr auto is_valid() const noexcept -> bool { return fields_are_valid() && components_are_valid(); }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void
    {
        prefetch_segments(prefetcher);
        segment_locator_.prefetch(prefetcher);
    }

private:
    constexpr auto extend_final_tangent(x_t dx_extended) const noexcept -> y_t
    {
        return segments_[segment_count_ - 1].extend_final_tangent(dx_extended);
    }

    constexpr auto fields_are_valid() const noexcept -> bool
    {
        // segment count must be in [1, max_segments]
        if (segment_count_ < 1 || max_segments < segment_count_) return false;

        // prev_segment_index_ must be in [0, segment_count_)
        if (prev_segment_index_ < 0 || segment_count_ <= prev_segment_index_) return false;

        return true;
    }

    constexpr auto components_are_valid() const noexcept -> bool
    {
        // dispatch to segment locator
        if (!segment_locator_.is_valid(segment_count_)) return false;

        // dispatch to each segment
        for (auto i = 0; i < segment_count_; ++i)
        {
            if (!segments_[i].is_valid()) return false;
        }

        return true;
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
    int_t segment_count_{};
    segment_locator_t segment_locator_{};

    // *must* be aligned or the prefetching scheme is useless
    alignas(64) segments_t segments_{};

    mutable int_t prev_segment_index_ = 0;
};

} // namespace crv::spline
