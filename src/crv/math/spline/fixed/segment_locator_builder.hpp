// SPDX-License-Identifier: MIT

/// \file
/// \brief builder for spline segment locator
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/spline/fixed/segment_locator.hpp>
#include <bit>
#include <span>

namespace crv::spline::fixed_point::segment_locator {

template <int_t depth_max, int_t node_key_count> struct row_offsets_t
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

/// branchless quaternary bfs tree builder
template <typename traits_t> class builder_t
{
public:
    using location_t = traits_t::location_t;
    using data_t = traits_t::data_t;

    static auto constexpr total_key_count = traits_t::total_key_count;

    constexpr auto operator()(std::span<location_t const, total_key_count> sorted_keys) noexcept -> data_t
    {
        static constexpr auto branching_mask = branching_factor - 1;

        data_t result;

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
            result.nodes[node_index].keys[key_offset_in_node] = sorted_keys[in_order_index - 1];
        }

        return result;
    }

private:
    static auto constexpr branching_factor = traits_t::branching_factor;
    static auto constexpr depth_max = traits_t::depth_max;
    static auto constexpr node_count = traits_t::node_count;
    static auto constexpr node_key_count = traits_t::node_key_count;

    using row_offsets_t = row_offsets_t<depth_max, node_key_count>;
    static constexpr auto row_offsets = row_offsets_t{};
};

} // namespace crv::spline::fixed_point::segment_locator
