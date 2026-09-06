// SPDX-License-Identifier: MIT

/// \file
/// \brief induced-gain segment construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <climits>
#include <concepts>
#include <limits>

namespace crv::spline {

/// creates the final runtime segment directly from its local-coordinate transfer cubic and exact fixed geometry
template <typename t_segment_t, typename segment_quantizer_t, typename segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;
    using cubic_t = segment_quantizer_t::cubic_t;
    using scalar_t = segment_quantizer_t::scalar_t;
    using packed_field_t = segment_packer_t::packed_field_t;

    static constexpr auto scalar_significand_bits = std::numeric_limits<scalar_t>::digits;
    static constexpr auto intermediate_mantissa_bits = static_cast<int_t>(sizeof(packed_field_t) * CHAR_BIT)
        - segment_packer_t::segment_layout.intermediate.shift_width;
    static constexpr auto intermediate_mantissa_magnitude_bits = intermediate_mantissa_bits - 1;

    static_assert(segment_packer_t::segment_layout == segment_t::segment_unpacker_t::segment_layout);
    static_assert(
        segment_packer_t::segment_layout.intermediate.max_shift() == segment_quantizer_t::max_intermediate_shift);
    static_assert(scalar_significand_bits <= intermediate_mantissa_magnitude_bits,
        "scalar significand does not fit intermediate packed mantissa");

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
