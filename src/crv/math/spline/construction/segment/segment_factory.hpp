// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic segment construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/float_extraction.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/segment.hpp>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// packing
// --------------------------------------------------------------------------------------------------------------------

/// packs segments tightly according to layout
template <typename t_packed_segment_t, typename unpacked_segment_t, typename field_packer_t, auto segment_layout>
struct segment_packer_t
{
    using packed_segment_t = t_packed_segment_t;

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

// --------------------------------------------------------------------------------------------------------------------
// orchestration
// --------------------------------------------------------------------------------------------------------------------

/// creates final segment from its cubic and width
template <typename t_segment_t, typename segment_quantizer_t, typename segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;

    using cubic_t = segment_quantizer_t::cubic_t;

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
