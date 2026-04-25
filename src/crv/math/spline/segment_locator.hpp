// SPDX-License-Identifier: MIT

/// \file
/// \brief spline segment locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <array>
#include <bit>
#include <span>

namespace crv::spline {

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
template <typename t_x_t, int t_depth_max> class segment_locator_t
{
public:
    using x_t = t_x_t;
    static constexpr auto depth_max = int_t{t_depth_max};

    static constexpr auto branching_factor = 4;
    static constexpr auto max_segment_count = 1 << (2 * depth_max);
    static constexpr auto total_key_count = max_segment_count - 1;
    static constexpr auto node_key_count = branching_factor - 1;
    static constexpr auto node_count = total_key_count / node_key_count;

    struct result_t
    {
        int_t index;
        x_t origin;

        auto operator<=>(result_t const&) const noexcept -> auto = default;
        auto operator==(result_t const&) const noexcept -> bool = default;
    };

    using node_keys_t = std::array<x_t, node_key_count>;
    struct alignas(32) node_t
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

    explicit constexpr segment_locator_t(std::span<x_t const, total_key_count> sorted_keys, x_t x_max) noexcept
        : x_max_{x_max}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_locator_t>);

        // walk tree in-order and place next sorted key into each position
        for (auto in_order_index = 1; in_order_index <= total_key_count; ++in_order_index) // 1-based
        {
            auto const node_location = node_location_t{in_order_index};
            nodes_[node_location.node_index].keys[node_location.key_offset] = sorted_keys[in_order_index - 1];
        }
    }

    constexpr auto locate(x_t x) const noexcept -> result_t
    {
        auto index = 0;
        auto origin = x_t{0};

        for (auto depth = 0; depth < depth_max; ++depth)
        {
            // alias keys locally in sorted order
            auto const key0 = nodes_[index].keys[0];
            auto const key1 = nodes_[index].keys[1];
            auto const key2 = nodes_[index].keys[2];

            // choose lower bound key
            origin = (x >= key0) ? key0 : origin;
            origin = (x >= key1) ? key1 : origin;
            origin = (x >= key2) ? key2 : origin;

            // choose lower bound offset
            auto const child_offset = (x >= key0) + (x >= key1) + (x >= key2);

            index = 4 * index + 1 + child_offset;
        }

        return {.index = index - node_count, .origin = origin};
    }

    /// end of final segment
    constexpr auto x_max() const noexcept -> x_t { return x_max_; }

    /// validates tree structure and capacity
    constexpr auto is_valid(int_t segment_count) const noexcept -> bool
    {
        // validate segment_count is in valid range
        if (segment_count <= 0 || max_segment_count < segment_count) return false;

        // validate x_max is nonnegative
        if (x_max_ <= x_t{0}) return false;

        auto previous_key = min<x_t>();

        // validate real breakpoints in sorted order
        for (auto i = 1; i < segment_count; ++i)
        {
            auto const key = key_at(i);
            if (key < x_t{0}) return false;
            if (key <= previous_key) return false;
            if (key >= x_max_) return false;
            previous_key = key;
        }

        // padding keys: must be >= x_max_ and remain monotonic so descent remains structurally sound
        for (auto i = segment_count; i <= total_key_count; ++i)
        {
            auto const key = key_at(i);
            if (key <= previous_key) return false;
            if (key < x_max_) return false;
            previous_key = key;
        }

        return true;
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void
    {
        // prefetch first two cache lines at head of tree; this includes the first two levels of comparison
        prefetcher.prefetch(&nodes_[0], 2);
    }

private:
    static constexpr auto row_offsets = row_offsets_t{};

    struct node_location_t
    {
        int_t node_index;
        int_t key_offset;

        constexpr node_location_t(int_t in_order_index) noexcept
        {
            static constexpr auto branching_mask = branching_factor - 1;

            // The relationship between an in-order index and its flat position in the tree is bijective, closed form.
            // The height is the number of trailing zeros in its quaternary representation, which is the number of pairs
            // of trailing zeros in its binary representation. The index must be one-based so that the root has the
            // maximum trailing-zero count rather than the degenerate all-zeros case.
            //
            // After stripping off the pairs of zeros, what remains is the offset of the key within the row. The bottom
            // 2 bits contain the offset of the key within its containing node, and the remaining bits define the offset
            // of the node within the row. The index of the node in the flat array is found relative to the offset to
            // the base index of the row.

            // height above floor; intrinsic to ordering, independent of tree layout
            auto const height_above_floor = std::countr_zero(int_cast<uint_t>(in_order_index)) >> 1;

            auto const key_offset_in_row = in_order_index >> (2 * height_above_floor); // strip trailing 0 pairs
            auto const node_offset_in_row = key_offset_in_row >> 2; // remaining bits above low 2

            // depth below root; a layout property depending on tree structure
            auto const depth_below_root = depth_max - 1 - height_above_floor;

            node_index = row_offsets.base_index[depth_below_root] + node_offset_in_row;
            key_offset = (key_offset_in_row & branching_mask) - 1; // low 2 bits, 0-based
        }
    };

    constexpr auto key_at(int_t in_order_index) const noexcept -> x_t
    {
        auto const node_location = node_location_t{in_order_index};
        return nodes_[node_location.node_index].keys[node_location.key_offset];
    }

    alignas(64) nodes_t nodes_;
    x_t x_max_;
};

} // namespace crv::spline
