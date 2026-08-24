// SPDX-License-Identifier: MIT

/// \file
/// \brief spline segment locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <array>
#include <bit>
#include <cassert>
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
    static constexpr auto leaf_base_index = []() constexpr {
        if constexpr (depth_max == 0) return int_t{0};
        else return ((int_t{1} << (2 * (depth_max - 1))) - 1) / node_key_count;
    }();

    struct result_t
    {
        int_t index;
        x_t origin;

        auto operator<=>(result_t const&) const noexcept -> auto = default;
        auto operator==(result_t const&) const noexcept -> bool = default;
    };

    struct hint_t
    {
        int_t segment_index{};
        x_t leaf_origin{};
        x_t leaf_end{};

        auto operator<=>(hint_t const&) const noexcept -> auto = default;
        auto operator==(hint_t const&) const noexcept -> bool = default;
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
            auto const& keys = nodes_[index].keys;

            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;

            auto const child_offset = (x >= keys[0]) + (x >= keys[1]) + (x >= keys[2]);
            index = branching_factor * index + 1 + child_offset;
        }

        return {.index = index - node_count, .origin = origin};
    }

    /// locates a segment and maintains a four-segment leaf hint
    constexpr auto locate(x_t x, hint_t& hint) const noexcept -> result_t
    {
        if constexpr (depth_max == 0)
        {
            hint = {.segment_index = 0, .leaf_origin = x_t{0}, .leaf_end = x_max_};
            return {.index = 0, .origin = x_t{0}};
        }
        else
        {
            if (hint.leaf_origin <= x && x < hint.leaf_end) return locate_in_leaf(x, hint);
            return locate_full(x, hint);
        }
    }

    /// number of real segments; the rest of the key array is padding
    constexpr auto segment_count() const noexcept -> int_t { return segment_count_; }

    /// end of final segment
    constexpr auto x_max() const noexcept -> x_t { return x_max_; }

    /// start of one active segment
    constexpr auto segment_origin(int_t segment_index) const noexcept -> x_t
    {
        assert(0 <= segment_index && segment_index < segment_count_ && "segment_locator_t: segment index out of bounds");
        return segment_index == 0 ? x_t{0} : key_at(segment_index);
    }

    /// end of one active segment
    constexpr auto segment_end(int_t segment_index) const noexcept -> x_t
    {
        assert(0 <= segment_index && segment_index < segment_count_ && "segment_locator_t: segment index out of bounds");
        return segment_index + 1 == segment_count_ ? x_max_ : key_at(segment_index + 1);
    }

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
            if (key <= x_t{0}) return false;
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

    /// prefetches the leaf selected by the previous lookup
    constexpr auto prefetch(hint_t const& hint, auto const& prefetcher) const noexcept -> void
    {
        if constexpr (depth_max != 0)
        {
            assert(0 <= hint.segment_index && hint.segment_index < max_segment_count
                && "segment_locator_t: hint index out of bounds");
            auto const leaf_index = hint.segment_index / branching_factor;
            prefetcher.prefetch(&nodes_[leaf_base_index + leaf_index]);
        }
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

    constexpr auto locate_full(x_t x, hint_t& hint) const noexcept -> result_t
    {
        auto index = int_t{0};
        auto origin = x_t{0};
        auto end = x_max_;

        for (auto depth = int_t{0}; depth < depth_max - 1; ++depth)
        {
            auto const& keys = nodes_[index].keys;

            origin = (x >= keys[0]) ? keys[0] : origin;
            origin = (x >= keys[1]) ? keys[1] : origin;
            origin = (x >= keys[2]) ? keys[2] : origin;
            end = (x < keys[2]) ? keys[2] : end;
            end = (x < keys[1]) ? keys[1] : end;
            end = (x < keys[0]) ? keys[0] : end;

            auto const child_offset = (x >= keys[0]) + (x >= keys[1]) + (x >= keys[2]);
            index = branching_factor * index + 1 + child_offset;
        }

        auto const leaf_index = index - leaf_base_index;
        auto const leaf_origin = origin;
        auto const leaf_end = end;
        auto const& keys = nodes_[index].keys;

        origin = (x >= keys[0]) ? keys[0] : origin;
        origin = (x >= keys[1]) ? keys[1] : origin;
        origin = (x >= keys[2]) ? keys[2] : origin;

        auto const child_offset = (x >= keys[0]) + (x >= keys[1]) + (x >= keys[2]);
        auto const segment_index = branching_factor * leaf_index + child_offset;

        hint = {.segment_index = segment_index, .leaf_origin = leaf_origin, .leaf_end = leaf_end};
        return {.index = segment_index, .origin = origin};
    }

    constexpr auto locate_in_leaf(x_t x, hint_t& hint) const noexcept -> result_t
    {
        assert(hint.leaf_origin <= x && x < hint.leaf_end && "segment_locator_t: input outside hinted leaf");
        assert(0 <= hint.segment_index && hint.segment_index < max_segment_count
            && "segment_locator_t: hint index out of bounds");

        auto const leaf_index = hint.segment_index / branching_factor;
        auto const& keys = nodes_[leaf_base_index + leaf_index].keys;
        auto origin = hint.leaf_origin;

        origin = (x >= keys[0]) ? keys[0] : origin;
        origin = (x >= keys[1]) ? keys[1] : origin;
        origin = (x >= keys[2]) ? keys[2] : origin;

        auto const child_offset = (x >= keys[0]) + (x >= keys[1]) + (x >= keys[2]);
        auto const segment_index = branching_factor * leaf_index + child_offset;
        hint.segment_index = segment_index;

        return {.index = segment_index, .origin = origin};
    }

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
