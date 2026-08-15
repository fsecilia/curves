// SPDX-License-Identifier: MIT

/// \file
/// \brief induced-gain segment construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>

namespace crv::spline {

/// creates the final runtime segment directly from its local-coordinate transfer cubic and exact fixed geometry
template <typename t_segment_t, typename segment_quantizer_t, typename segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;
    using cubic_t = segment_quantizer_t::cubic_t;

    static_assert(segment_packer_t::segment_layout == segment_t::segment_unpacker_t::segment_layout);
    static_assert(
        segment_packer_t::segment_layout.intermediate.max_shift() == segment_quantizer_t::max_intermediate_shift);

    [[no_unique_address]] segment_quantizer_t quantize_segment;
    [[no_unique_address]] segment_packer_t pack_segment;

    constexpr auto operator()(
        cubic_t const& cubic, typename segment_t::x_t width, typename segment_t::x_t x0) const noexcept -> segment_t
    {
        static_assert(std::same_as<typename segment_quantizer_t::x_t, typename segment_t::x_t>);
        auto const unpacked_segment = quantize_segment(cubic, width, x0);
        auto const packed_segment = pack_segment(unpacked_segment);
        return segment_t{packed_segment};
    }
};

} // namespace crv::spline
