// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic segment construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline {

/// creates final segment from its cubic and width
template <typename t_segment_t, typename segment_quantizer_t, typename segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;

    using cubic_t = segment_quantizer_t::cubic_t;

    // enforce identical layouts
    //
    // All three layout consumers, quantizer, packer, and segment's unpacker, must agree on the same segment_layout, or
    // encoding and decoding desynchronize.
    static_assert(segment_packer_t::segment_layout == segment_t::segment_unpacker_t::segment_layout);
    static_assert(
        segment_packer_t::segment_layout.intermediate.max_shift() == segment_quantizer_t::max_intermediate_shift);

    [[no_unique_address]] segment_quantizer_t quantize_segment;
    [[no_unique_address]] segment_packer_t pack_segment;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> segment_t
    {
        auto const unpacked_segment = quantize_segment(cubic, log2_width);
        auto const packed_segment = pack_segment(unpacked_segment);
        return segment_t{packed_segment};
    }
};

} // namespace crv::spline
