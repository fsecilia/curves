// SPDX-License-Identifier: MIT

/// \file
/// \brief spline segment locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <array>
#include <bit>
#include <new>
#include <span>

namespace crv::spline::fixed_point {

/// branchless quaternary bfs tree over spline segments
///
/// A binary tree is a k-ary tree with 2 branches and 1 condition. A quaternary tree is a k-ary tree with 4 branches and
/// 3 conditions.
///
/// This implementation is branchless, so as it descends through the tree, it chooses the next child arithmetically,
/// rather than conditionally. This avoids branch mispredictions at the cost of data dependency. It is naturally
/// shallower than an equivalent binary tree, so requires fewer iterations, though it may perform more comparisons
/// overall. It performs 3 comparisons per fetch, so it must fetch fewer times than a binary tree. Each node stores 3
/// keys, and the top nodes of the tree are stored adjacently, so the first cache line contains the first few conditions
/// with a single fetch.
template <typename t_location_t, int t_depth_max> class segment_locator_t
{
public:
    using location_t = t_location_t;
    static constexpr auto depth_max = int_t{t_depth_max};

    static constexpr auto branching_factor = 4;
    static constexpr auto max_segment_count = 1 << (2 * depth_max);
    static constexpr auto total_key_count = max_segment_count - 1;
    static constexpr auto node_key_count = branching_factor - 1;
    static constexpr auto node_count = total_key_count / node_key_count;
    static constexpr auto cache_line_size = std::hardware_constructive_interference_size;

    struct result_t
    {
        int_t index;
        location_t x_left;

        auto operator<=>(result_t const&) const noexcept -> auto = default;
        auto operator==(result_t const&) const noexcept -> bool = default;
    };

    using node_keys_t = std::array<location_t, node_key_count>;
    struct alignas(cache_line_size / 2) node_t
    {
        node_keys_t keys;

        auto operator<=>(node_t const&) const noexcept -> auto = default;
        auto operator==(node_t const&) const noexcept -> bool = default;
    };

    using nodes_t = std::array<node_t, node_count>;

    struct row_offsets_t
    {
        /// index in node array of the first node at each depth
        std::array<int_t, depth_max> base_index; // (4^depth - 1)/3

        constexpr row_offsets_t() noexcept
        {
            for (auto depth = 0; depth < depth_max; ++depth)
            {
                base_index[depth] = ((1 << (2 * depth)) - 1) / node_key_count;
            }
        }
    };

    constexpr auto operator()(location_t x) const noexcept -> result_t
    {
        auto index = 0;
        auto x_left = location_t{0};

        for (auto depth = 0; depth < depth_max; ++depth)
        {
            // alias keys locally in sorted order
            auto const key0 = nodes_[index].keys[0];
            auto const key1 = nodes_[index].keys[1];
            auto const key2 = nodes_[index].keys[2];

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

    explicit constexpr segment_locator_t(std::span<location_t const, total_key_count> sorted_keys) noexcept
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_locator_t>);

        static constexpr auto branching_mask = branching_factor - 1;

        // walk tree in-order and place next sorted key into each position
        //
        // The relationship between an in-order index and its flat position in the tree is bijective, closed form. The
        // height is the number of trailing zeros in its quaternary representation, which is the number of pairs of
        // trailing zeros in its binary representation. The index must be one-based so that the root has the maximum
        // trailing-zero count rather than the degenerate all-zeros case. After stripping off the pairs of zeros, what
        // remains is the offset of the key within the row. The bottom 2 bits contain the offset of the key within its
        // containing node, and the remaining bits define the offset of the node within the row. The index of the node
        // in the flat array is found relative to the offset to the base index of the row.
        for (auto in_order_index = 1; in_order_index <= total_key_count; ++in_order_index) // 1-based
        {
            // height above floor; intrinsic to ordering, independent of tree layout
            auto const height_above_floor = std::countr_zero(int_cast<uint_t>(in_order_index)) >> 1; // trailing 0 pairs

            auto const key_offset_in_row = in_order_index >> (2 * height_above_floor); // strip trailing 0 pairs
            auto const key_offset_in_node = (key_offset_in_row & branching_mask) - 1; // low 2 bits, 0-based
            auto const node_offset_in_row = key_offset_in_row >> 2; // remaining bits above low 2

            // depth below root; a layout property depending on tree structure
            auto const depth_below_root = depth_max - 1 - height_above_floor; // invert relative to depth, 0-based

            auto const node_index = row_offsets.base_index[depth_below_root] + node_offset_in_row; // row base + offset
            nodes_[node_index].keys[key_offset_in_node] = sorted_keys[in_order_index - 1];
        }
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void { prefetcher.prefetch(&nodes_[0]); }

private:
    static constexpr auto row_offsets = row_offsets_t{};

    alignas(cache_line_size) nodes_t nodes_;
};

} // namespace crv::spline::fixed_point
