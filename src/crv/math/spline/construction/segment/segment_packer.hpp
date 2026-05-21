// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic segment packing
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline {

/// packs segments tightly according to layout
template <typename t_packed_segment_t, typename unpacked_segment_t, typename field_packer_t, auto t_segment_layout>
struct segment_packer_t
{
    using packed_segment_t = t_packed_segment_t;

    static constexpr auto segment_layout = t_segment_layout;

    [[no_unique_address]] field_packer_t pack_field;

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment) const noexcept -> packed_segment_t
    {
        return packed_segment_t{
            pack_field(unpacked_segment[0], segment_layout.intermediate),
            pack_field(unpacked_segment[1], segment_layout.intermediate),
            pack_field(unpacked_segment[2], segment_layout.intermediate),
            pack_field(unpacked_segment[3], segment_layout.final),
        };
    }
};

} // namespace crv::spline
