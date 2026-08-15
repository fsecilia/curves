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
/// Each node stores three keys and chooses among four children arithmetically. Compared with a binary tree this trades
/// more comparisons per node for fewer levels and no branch prediction. The upper nodes are adjacent, so the first
/// cache line covers the first few decisions.
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

    constexpr segment_locator_t() noexcept : x_max_{}, segment_count_{}, nodes_{} {}

    explicit constexpr segment_locator_t(
        std::span<x_t const, total_key_count> sorted_keys, x_t x_max, int_t segment_count) noexcept
        : x_max_{x_max}, segment_count_{segment_count}
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

    /// number of real segments; the rest of the key array is padding
    constexpr auto segment_count() const noexcept -> int_t { return segment_count_; }

    /// end of final segment
    constexpr auto x_max() const noexcept -> x_t { return x_max_; }

    /// validates tree structure and capacity
    constexpr auto is_valid() const noexcept -> bool
    {
        // validate segment count
        if (segment_count_ <= 0 || max_segment_count < segment_count_) return false;

        // validate domain end
        if (x_max_ <= x_t{0}) return false;

        auto previous_key = min<x_t>();

        // validate sorted real breakpoints
        for (auto i = 1; i < segment_count_; ++i)
        {
            auto const key = key_at(i);
            if (key < x_t{0}) return false;
            if (key <= previous_key) return false;
            if (key >= x_max_) return false;
            previous_key = key;
        }

        // keep padding at or past domain end
        for (auto i = segment_count_; i <= total_key_count; ++i)
        {
            auto const key = key_at(i);
            if (key < previous_key) return false;
            if (key < x_max_) return false;
            previous_key = key;
        }

        return true;
    }

    constexpr auto prefetch(auto const& prefetcher) const noexcept -> void
    {
        // prefetch first two cache lines covering the top levels
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

            // map in-order key index to flat tree position
            //
            // In a one-based in-order index, each pair of trailing zero bits raises the key one quaternary level. After
            // removing those pairs, the low two bits select the key inside its node and the remaining bits select the
            // node within that row. Adding the row base gives the flat-array position.

            // height from in-order trailing zeros
            auto const height_above_floor = std::countr_zero(int_cast<uint_t>(in_order_index)) >> 1;

            auto const key_offset_in_row = in_order_index >> (2 * height_above_floor); // strip trailing 0 pairs
            auto const node_offset_in_row = key_offset_in_row >> 2; // remaining bits above low 2

            // convert height to tree depth
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

    x_t x_max_;
    int_t segment_count_;
    alignas(64) nodes_t nodes_;
};

} // namespace crv::spline
