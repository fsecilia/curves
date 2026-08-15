// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic induced-gain segment packing
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline {

/// packs the three dynamic S(u) fields while preserving g0 as an ordinary y_t ordinate
template <typename t_packed_segment_t, typename unpacked_segment_t, typename field_packer_t, auto t_segment_layout>
struct segment_packer_t
{
    using packed_segment_t = t_packed_segment_t;

    static constexpr auto segment_layout = t_segment_layout;

    [[no_unique_address]] field_packer_t pack_field;

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment) const noexcept -> packed_segment_t
    {
        return packed_segment_t{
            .d = pack_field(unpacked_segment.d, segment_layout.intermediate),
            .c = pack_field(unpacked_segment.c, segment_layout.intermediate),
            .b = pack_field(unpacked_segment.b, segment_layout.final),
            .g0 = unpacked_segment.g0,
        };
    }
};

} // namespace crv::spline
