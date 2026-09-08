// SPDX-License-Identifier: MIT

/// \file
/// \brief induced-gain segment construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/spline/construction/error.hpp>
#include <climits>
#include <concepts>
#include <expected>
#include <limits>

namespace crv::spline {

/// creates the final runtime segment from its local cubic, exact left endpoint derivative, and fixed geometry
template <typename t_segment_t, typename segment_quantizer_t, typename segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;
    using cubic_t = segment_quantizer_t::cubic_t;
    using scalar_t = segment_quantizer_t::scalar_t;
    using x_t = segment_t::x_t;
    using packed_field_t = segment_packer_t::packed_field_t;
    using error_t = spline_construction_error_t<x_t>;
    using result_t = std::expected<segment_t, error_t>;

    static constexpr auto scalar_significand_bits = std::numeric_limits<scalar_t>::digits;
    static constexpr auto intermediate_significand_bits = static_cast<int_t>(sizeof(packed_field_t) * CHAR_BIT)
        - segment_packer_t::segment_layout.intermediate.shift_width;
    static constexpr auto intermediate_significand_magnitude_bits = intermediate_significand_bits - 1;

    static_assert(segment_packer_t::segment_layout == segment_t::segment_unpacker_t::segment_layout);
    static_assert(
        segment_packer_t::segment_layout.intermediate.max_shift() == segment_quantizer_t::max_intermediate_shift);
    static_assert(scalar_significand_bits <= intermediate_significand_magnitude_bits,
        "scalar significand does not fit intermediate packed significand");

    [[no_unique_address]] segment_quantizer_t quantize_segment;
    [[no_unique_address]] segment_packer_t pack_segment;

    constexpr auto operator()(cubic_t const& cubic, scalar_t left_endpoint_derivative, x_t width, x_t x0) const noexcept
        -> result_t
    {
        static_assert(std::same_as<typename segment_quantizer_t::x_t, x_t>);
        auto const right = x0 + width;

        if (!quantize_segment.is_g0_representable(cubic, x0))
        {
            return std::unexpected{error_t{
                .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
                .left = x0,
                .right = right,
            }};
        }

        auto const unpacked_segment = quantize_segment(cubic, left_endpoint_derivative, width, x0);
        if (!segment_packer_t::segment_layout.final.is_encodable(unpacked_segment.b))
        {
            return std::unexpected{error_t{
                .reason = spline_construction_error_reason_t::left_endpoint_derivative_not_representable,
                .left = x0,
                .right = right,
            }};
        }

        return segment_t{pack_segment(unpacked_segment)};
    }
};

} // namespace crv::spline
