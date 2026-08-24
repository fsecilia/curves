// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point induced-gain spline
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <array>
#include <cassert>
#include <type_traits>

namespace crv::spline {

/// fixed-point spline evaluating gain induced by local transfer Hermite cubics over a specific domain
template <typename t_segment_t, typename t_extended_tangent_t, typename t_segment_locator_t> struct spline_t
{
    using segment_t = t_segment_t;
    using extended_tangent_t = t_extended_tangent_t;
    using segment_locator_t = t_segment_locator_t;

    using x_t = segment_t::x_t;
    using y_t = segment_t::y_t;
    using hint_t = segment_locator_t::hint_t;

    static constexpr auto max_segment_count = segment_locator_t::max_segment_count;
    using segments_t = std::array<segment_t, max_segment_count>;

    segment_locator_t segment_locator{};
    alignas(64) segments_t segments{};
    extended_tangent_t extend_final_tangent{};

    /// \pre 0 <= x
    /// This function is marked always_inline because the mangled name is too long and breaks objtool.
    CRV_ALWAYS_INLINE constexpr auto evaluate(x_t x, hint_t& hint) const noexcept -> y_t
    {
        assert(x_t{0} <= x && "spline_t: input out of bounds");

        auto const x_max = segment_locator.x_max();
        if (x >= x_max) return extend_final_tangent(x - x_max);

        auto const location = segment_locator.locate(x, hint);
        assert(0 <= location.index && location.index < segment_locator.segment_count()
            && "spline_t: located segment index out of bounds");
        assert(0 <= location.origin && location.origin <= x && "spline_t: located segment origin out of range");

        return segments[location.index](x, location.origin);
    }

    /// validates data the driver receives
    constexpr auto is_valid() const noexcept -> bool
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<spline_t>);
        static_assert(std::is_standard_layout_v<spline_t>);
        return segment_locator.is_valid();
    }

    constexpr auto prefetch(hint_t const& hint, auto const& prefetcher) const noexcept -> void
    {
        segment_locator.prefetch(hint, prefetcher);
        prefetch_segments(hint, prefetcher);
    }

private:
    /// prefetches the last selected segment and its neighbors
    ///
    /// Mouse velocity usually stays near its previous segment, so these are the most likely next cache lines.
    auto prefetch_segments(hint_t const& hint, auto const& prefetcher) const noexcept -> void
    {
        assert(
            0 <= hint.segment_index && hint.segment_index < max_segment_count && "spline_t: hint index out of bounds");

        // these casts are required to prevent ub when forming addresses outside of the array
        auto const base_address = reinterpret_cast<std::uintptr_t>(segments.data());
        auto const offset = sizeof(segment_t);

        // prefetch current segment and neighbors
        //
        // Cache-line alignment lets two addresses cover all three segments. At either end, one address may land in
        // neighboring payload storage; prefetch tolerates that, so no boundary branches are needed.
        prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (hint.segment_index - 1) * offset));
        prefetcher.prefetch(reinterpret_cast<void const*>(base_address + (hint.segment_index + 1) * offset));
    }
};

} // namespace crv::spline
