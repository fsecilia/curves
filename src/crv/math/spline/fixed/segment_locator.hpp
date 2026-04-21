// SPDX-License-Identifier: MIT

/// \file
/// \brief spline segment locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <new>

namespace crv::spline::fixed_point::segment_locator {

template <typename location_t, int_t node_count, int_t node_key_count> struct data_t
{
    static constexpr auto cache_line_size = std::hardware_constructive_interference_size;
    struct alignas(cache_line_size / 2) node_t
    {
        using keys_t = std::array<location_t, node_key_count>;
        keys_t keys;

        auto operator<=>(node_t const&) const noexcept -> auto = default;
        auto operator==(node_t const&) const noexcept -> bool = default;
    };

    alignas(cache_line_size) std::array<node_t, node_count> nodes;

    auto operator<=>(data_t const&) const noexcept -> auto = default;
    auto operator==(data_t const&) const noexcept -> bool = default;
};

template <typename location_t> struct result_t
{
    int_t index;
    location_t x_left;

    auto operator<=>(result_t const&) const noexcept -> auto = default;
    auto operator==(result_t const&) const noexcept -> bool = default;
};

template <typename t_location_t, int t_depth_max> struct traits_t
{
    using location_t = t_location_t;

    static constexpr auto depth_max = int_t{t_depth_max};

    static constexpr auto branching_factor = 4;
    static constexpr auto max_segment_count = 1 << (2 * depth_max);
    static constexpr auto total_key_count = max_segment_count - 1;
    static constexpr auto node_key_count = branching_factor - 1;
    static constexpr auto node_count = total_key_count / node_key_count;

    using data_t = data_t<location_t, node_count, node_key_count>;
    using result_t = result_t<location_t>;
};

/// branchless quaternary bfs tree over spline segments
///
/// This indexing scheme has a slightly longer runtime than something branchless, but it has no jitter and loads fewer
/// cache lines, so has fewer cache misses than a more speculative structure.
template <typename traits_t> class locator_t
{
public:
    using data_t = traits_t::data_t;
    using location_t = traits_t::location_t;
    using result_t = traits_t::result_t;

    static auto constexpr depth_max = traits_t::depth_max;
    static auto constexpr node_count = traits_t::node_count;

    explicit constexpr locator_t(data_t&& data) noexcept : data_{std::move(data)}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<locator_t>);
    }

    constexpr auto operator()(location_t x) const noexcept -> result_t
    {
        auto index = 0;
        auto x_left = location_t{0};

        for (auto depth = 0; depth < depth_max; ++depth)
        {
            // each node contains keys in sorted order.
            auto const key0 = data_.nodes[index].keys[0];
            auto const key1 = data_.nodes[index].keys[1];
            auto const key2 = data_.nodes[index].keys[2];

            // choose lower bound key
            x_left = (x >= key0) ? key0 : x_left;
            x_left = (x >= key1) ? key1 : x_left;
            x_left = (x >= key2) ? key2 : x_left;

            // choose lower bound offset
            auto const child_offset = (x >= key0) + (x >= key1) + (x >= key2);

            index = 4 * index + 1 + child_offset;
        }

        return {.index = index - node_count, .x_left = x_left};
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void { prefetcher.prefetch(&data_.nodes[0]); }

private:
    data_t data_;
};

} // namespace crv::spline::fixed_point::segment_locator
