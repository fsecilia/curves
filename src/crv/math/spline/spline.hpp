// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic spline
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <array>
#include <cassert>

namespace crv::spline {

/// fixed-point cubic spline approximating a function over a specific domain
template <typename t_segment_t, typename t_segment_locator_t> class spline_t
{
public:
    using segment_t = t_segment_t;
    using segment_locator_t = t_segment_locator_t;

    using x_t = segment_t::x_t;
    using y_t = segment_t::y_t;

    static constexpr auto max_segment_count = segment_locator_t::max_segment_count;

    using segments_t = std::array<segment_t, max_segment_count>;

    constexpr spline_t() noexcept : segment_locator_{}, segments_{}, prev_segment_index_{} {}

    /// \pre 0 < locator.segment_count() <= max_segments
    constexpr spline_t(segment_locator_t const& locator, segments_t const& segments,
        segment_t const& extended_tangent_segment, int_t prev_segment_index = 0) noexcept
        : segment_locator_{locator}, segments_{segments}, extend_final_tangent_{extended_tangent_segment},
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

        // this will need to change to use an extension segment that isn't part of the array
        if (x >= x_max) return extend_final_tangent_(x - x_max);

        auto const location = segment_locator_.locate(x);
        assert(0 <= location.index && location.index < segment_locator_.segment_count()
            && "spline_t: located segment index out of bounds");
        assert(0 <= location.origin && location.origin <= x && "spline_t: located segment origin out of range");

        if !consteval { prev_segment_index_ = location.index; }

        auto const& segment = segments_[location.index];
        return segment(x - location.origin);
    }

    /// validates data the driver receives
    constexpr auto is_valid() const noexcept -> bool
    {
        auto const segment_count = segment_locator_.segment_count();

        // prev_segment_index_ must be in [0, locator.segment_count()); locator.is_valid() owns the count's range check
        if (prev_segment_index_ < 0 || segment_count <= prev_segment_index_) return false;

        // dispatch to segment locator
        if (!segment_locator_.is_valid()) return false;

        return true;
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void
    {
        prefetch_segments(prefetcher);
        segment_locator_.prefetch(prefetcher);
        prefetcher.prefetch(&extend_final_tangent_);
    }

private:
    /// prefetches the most recently selected segment and the two adjacent
    ///
    /// Prefetching these 3 segments serves as our hint to exploit the natural temporal locality of mouse velocity.
    auto prefetch_segments(auto const& prefetcher) const noexcept -> void
    {
        // these casts are required to prevent ub when forming addresses outside of the array
        auto const base_address = reinterpret_cast<std::uintptr_t>(segments_.data());
        auto const offset = sizeof(segment_t);

        // prefetch most recent segment
        //
        // Because segments_ is aligned to a cache line, one of these two will contain the cache line containing the
        // previous segment and one adjacent, and the other will contain the other adjacent. If prev_segment_index is 0
        // or segment_count_ - 1, these both technically are out of bounds, either prefetching the end of the segment
        // locator or whatever follows this type as a whole, but prefetching is built to be resiliant to this pattern.
        // We do not have to guard it with an condition, saving a pair of misprediction sources.
        prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (prev_segment_index_ - 1) * offset));
        prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (prev_segment_index_ + 1) * offset));
    }

    // these are ordered for overall cache-friendliness in operator ()
    segment_locator_t segment_locator_{};
    alignas(64) segments_t segments_{}; // *must* be aligned or the prefetching scheme is useless
    segment_t extend_final_tangent_{};

    mutable int_t prev_segment_index_ = 0;
};

} // namespace crv::spline
