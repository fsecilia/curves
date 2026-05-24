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
template <typename t_segment_t, typename t_extended_tangent_t, typename t_segment_locator_t> class spline_t
{
public:
    using segment_t = t_segment_t;
    using extended_tangent_t = t_extended_tangent_t;
    using segment_locator_t = t_segment_locator_t;

    using x_t = segment_t::x_t;
    using y_t = segment_t::y_t;

    static constexpr auto max_segment_count = segment_locator_t::max_segment_count;

    using segments_t = std::array<segment_t, max_segment_count>;

    /// wire format
    struct payload_t
    {
        // these are ordered for overall cache-friendliness in operator ()
        segment_locator_t segment_locator{};
        alignas(64) segments_t segments{}; // *must* be aligned or the prefetching scheme is useless
        extended_tangent_t extend_final_tangent{};
    };

    /// public payload
    ///
    /// This type is large, at least 10kiB. Making this data public saves an extra 10k copy on assign. It can't be a
    /// pointer or it will cause cache misses in the kernel. The composed types protect their own invariants.
    payload_t payload{};

    constexpr spline_t() noexcept = default;
    constexpr spline_t(payload_t payload) noexcept : payload{std::move(payload)} {}

    /// \pre 0 <= x
    constexpr auto operator()(x_t x) const noexcept -> y_t
    {
        assert(x_t{0} <= x && "spline_t: input out of bounds");

        auto const x_max = payload.segment_locator.x_max();

        // this will need to change to use an extension segment that isn't part of the array
        if (x >= x_max) return payload.extend_final_tangent(x - x_max);

        auto const location = payload.segment_locator.locate(x);
        assert(0 <= location.index && location.index < payload.segment_locator.segment_count()
            && "spline_t: located segment index out of bounds");
        assert(0 <= location.origin && location.origin <= x && "spline_t: located segment origin out of range");

        if !consteval { prev_segment_index_ = location.index; }

        auto const& segment = payload.segments[location.index];
        return segment(x - location.origin);
    }

    /// validates data the driver receives
    constexpr auto is_valid() const noexcept -> bool
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<spline_t>);

        auto const segment_count = payload.segment_locator.segment_count();

        // prev_segment_index_ must be in [0, locator.segment_count()); locator.is_valid() owns the count's range check
        if (prev_segment_index_ < 0 || segment_count <= prev_segment_index_) return false;

        // dispatch to segment locator
        if (!payload.segment_locator.is_valid()) return false;

        return true;
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void
    {
        prefetch_segments(prefetcher);
        payload.segment_locator.prefetch(prefetcher);
        prefetcher.prefetch(&payload.extend_final_tangent);
    }

private:
    /// prefetches the most recently selected segment and the two adjacent
    ///
    /// Prefetching these 3 segments serves as our hint to exploit the natural temporal locality of mouse velocity.
    auto prefetch_segments(auto const& prefetcher) const noexcept -> void
    {
        // these casts are required to prevent ub when forming addresses outside of the array
        auto const base_address = reinterpret_cast<std::uintptr_t>(payload.segments.data());
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

    mutable int_t prev_segment_index_ = 0;
};

} // namespace crv::spline
