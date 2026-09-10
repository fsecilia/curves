// SPDX-License-Identifier: MIT

/// \file
/// \brief assembles completed intervals into segments and populates segment_locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/spline/construction/error.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <expected>
#include <iterator>

namespace crv::spline {

/// sorts intervals in place by increasing x
struct interval_sorter_t
{
    constexpr auto operator()(auto& intervals) const noexcept -> void
    {
        std::ranges::sort(
            intervals, std::ranges::less{}, [](auto const& interval) noexcept { return interval.subdomain.left_x; });
    }
};

/// unzips intervals into segments and keys
struct interval_unzipper_t
{
    template <typename segments_t, typename keys_t>
    constexpr auto operator()(
        auto const& intervals, int_t segment_count, segments_t& segments, keys_t& keys) const noexcept -> void
    {
        segments[0] = intervals[0].segment;
        for (auto segment_index = 1; segment_index < segment_count; ++segment_index)
        {
            auto const& interval = intervals[segment_index];
            segments[segment_index] = interval.segment;
            keys[segment_index - 1] = interval.subdomain.left_x;
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
    using x_t = interval_t::segment_t::x_t;
    using error_t = spline_construction_error_t<x_t>;
    using result_t = std::expected<void, error_t>;
    using tangent_result_t = tangent_extender_t::result_t;
    using extended_tangent_t = tangent_result_t::value_type;

    static_assert(std::same_as<typename tangent_extender_t::error_t, error_t>);

    [[no_unique_address]] interval_sorter_t sort_intervals;
    [[no_unique_address]] interval_unzipper_t unzip_intervals;
    [[no_unique_address]] key_padder_t pad_keys;
    [[no_unique_address]] tangent_extender_t extend_tangent;

    template <typename spline_t> constexpr auto operator()(typestate_t&& state, spline_t& spline) const -> result_t
    {
        auto& completed_intervals = state.workspace.completed_intervals;

        auto const extended_tangent = prepare<spline_t>(completed_intervals);
        if (!extended_tangent) return std::unexpected{extended_tangent.error()};

        commit(completed_intervals, *extended_tangent, spline);
        return {};
    }

private:
    template <typename spline_t> constexpr auto prepare(auto& completed_intervals) const -> tangent_result_t
    {
        using segment_locator_t = spline_t::segment_locator_t;

        static_assert(segment_locator_t::total_key_count + 1 == spline_t::max_segment_count);

        assert(!completed_intervals.empty());

        auto const segment_count = int_cast<int_t>(std::size(completed_intervals));
        assert(segment_count <= segment_locator_t::max_segment_count);

        sort_intervals(completed_intervals);
        return extend_tangent(completed_intervals[segment_count - 1]);
    }

    template <typename spline_t>
    constexpr auto commit(auto& completed_intervals, extended_tangent_t const& extended_tangent, spline_t& spline) const
        -> void
    {
        using segment_locator_t = spline_t::segment_locator_t;
        using sorted_keys_t = std::array<x_t, segment_locator_t::total_key_count>;

        auto const segment_count = int_cast<int_t>(std::size(completed_intervals));
        auto sorted_keys = sorted_keys_t{};

        unzip_intervals(completed_intervals, segment_count, spline.segments, sorted_keys);
        pad_keys(sorted_keys, segment_count - 1, x_max);

        spline.segment_locator = segment_locator_t{sorted_keys, x_max, segment_count};
        spline.extend_final_tangent = extended_tangent;
        completed_intervals.clear();
    }

    static constexpr auto x_max = x_t{domain_end};
};

} // namespace crv::spline
