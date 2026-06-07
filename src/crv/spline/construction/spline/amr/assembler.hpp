// SPDX-License-Identifier: MIT

/// \file
/// \brief assembles completed intervals into segments and populate segment_locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <algorithm>
#include <iterator>

namespace crv::spline {

/// sorts intervals in place by increasing x
struct interval_sorter_t
{
    constexpr auto operator()(auto& intervals) const noexcept -> void
    {
        std::ranges::sort(
            intervals, std::ranges::less{}, [](auto const& interval) noexcept { return interval.subdomain.left.x; });
    }
};

/// unzips intervals into segments and keys
struct interval_unzipper_t
{
    template <typename segments_t, typename keys_t>
    constexpr auto operator()(
        auto const& intervals, int_t segment_count, segments_t& segments, keys_t& keys) const noexcept -> void
    {
        using segment_t = segments_t::value_type;
        using x_t = segment_t::x_t;

        segments[0] = intervals[0].segment;
        for (auto segment_index = 1; segment_index < segment_count; ++segment_index)
        {
            auto const& interval = intervals[segment_index];
            segments[segment_index] = interval.segment;
            keys[segment_index - 1] = to_fixed<x_t>(interval.subdomain.left.x);
        }
    }
};

/// pads end of sorted key array with max value
struct key_padder_t
{
    constexpr auto operator()(auto& keys, int_t start, auto const& value) const noexcept -> void
    {
        auto const size = int_cast<int_t>(std::size(keys));
        for (auto padding_index = start; padding_index < size; ++padding_index) keys[padding_index] = value;
    }
};

/// assembles completed intervals into final spline
template <typename typestate_t, typename interval_t, typename interval_sorter_t, typename interval_unzipper_t,
    typename key_padder_t, typename tangent_extender_t, int_t domain_end>
struct assembler_t
{
    [[no_unique_address]] interval_sorter_t sort_intervals;
    [[no_unique_address]] interval_unzipper_t unzip_intervals;
    [[no_unique_address]] key_padder_t pad_keys;
    [[no_unique_address]] tangent_extender_t extend_tangent;

    template <typename spline_t> constexpr auto operator()(typestate_t&& state, spline_t& spline) const -> void
    {
        using segment_locator_t = spline_t::segment_locator_t;

        constexpr auto total_key_count = segment_locator_t::total_key_count;
        static_assert(total_key_count + 1 == spline_t::max_segment_count);

        // get completed intervals out of workspace
        auto& workspace = state.workspace;
        auto& completed_intervals = workspace.completed_intervals;
        assert(!completed_intervals.empty());

        auto const segment_count = int_cast<int_t>(std::size(completed_intervals));
        assert(segment_count <= segment_locator_t::max_segment_count);

        // write to segments directly, in-place
        auto& segments = spline.payload.segments;

        // segment_locator_t does not yet support in-place construction, so create its ctor params locally
        using sorted_keys_t = std::array<x_t, total_key_count>;
        sorted_keys_t sorted_keys;

        // unzip sorted intervals and pad remaining keys
        sort_intervals(completed_intervals);
        unzip_intervals(completed_intervals, segment_count, segments, sorted_keys);
        pad_keys(sorted_keys, segment_count - 1, x_max);

        // write remaining fields to payload
        spline.payload.segment_locator = segment_locator_t{sorted_keys, x_max, segment_count};
        spline.payload.extend_final_tangent = extend_tangent(completed_intervals[segment_count - 1]);

        // mark intervals as consumed
        completed_intervals.clear();
    }

private:
    using segment_t = interval_t::segment_t;
    using x_t = segment_t::x_t;

    static constexpr auto x_max = x_t{domain_end};
};

} // namespace crv::spline
