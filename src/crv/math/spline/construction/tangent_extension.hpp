// SPDX-License-Identifier: MIT

/// \file
/// \brief segment tangent extension
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/spline/segment.hpp>
#include <climits>
#include <cmath>

namespace crv::spline {

/// generates linear segment matching final tangent of final mapped segment
template <typename t_interval_t, typename t_segment_t, typename segment_quantizer_t, typename t_segment_packer_t,
    typename t_field_packer_t, typename saturating_shifter_t, segment_layout_t segment_layout>
struct tangent_extender_t
{
    using interval_t = t_interval_t;
    using segment_t = t_segment_t;
    using segment_packer_t = t_segment_packer_t;
    using field_packer_t = t_field_packer_t;

    using x_t = segment_t::x_t;
    using unpacked_field_t = segment_quantizer_t::unpacked_field_t;
    using mantissa_t = unpacked_field_t::mantissa_t;
    using scalar_t = segment_quantizer_t::scalar_t;
    using cubic_t = segment_quantizer_t::cubic_t;

    [[no_unique_address]] segment_quantizer_t quantize_segment;
    [[no_unique_address]] segment_packer_t pack_segment;
    [[no_unique_address]] field_packer_t pack_field;
    [[no_unique_address]] saturating_shifter_t saturating_shifter;

    constexpr auto operator()(interval_t const& interval, segment_t const& segment) const noexcept -> segment_t
    {
        auto const log2_x_max
            = sizeof(typename x_t::value_t) * CHAR_BIT - x_t::frac_bits - is_signed_v<typename x_t::value_t>;

        auto const t = scalar_t{1};
        auto const dt_dx = std::ldexp(scalar_t{1}, int_cast<int>(log2_x_max - interval.subdomain.log2_width));
        auto const dy_dx_extended_segment = interval.cubic(jet_t{t, dt_dx}).df;

        // find fixed y at dx = width
        auto const final_segment_width = x_t{1} << interval.subdomain.log2_width;
        auto const y1_actual = segment(final_segment_width);

        // pack proto and unpack to find shift
        auto const y = from_fixed<scalar_t>(y1_actual);
        auto const tangent_cubic = cubic_t{0, 0, dy_dx_extended_segment, y};
        auto const unpacked_segment = quantize_segment(tangent_cubic, log2_x_max);
        auto packed_segment = pack_segment(unpacked_segment);
        auto const c3_shift = unpacked_segment[3].shift;

        // solve target mantissa directly via renormalization
        auto const y1_as_signed = saturate_cast<mantissa_t>(y1_actual.value);
        auto const target_mantissa = saturating_shifter.shift(y1_as_signed, c3_shift);

        // repack new c3
        packed_segment[fields_per_segment - 1]
            = pack_field(unpacked_field_t{.mantissa = target_mantissa, .shift = c3_shift}, segment_layout.final);
        return segment_t{packed_segment};
    }
};

} // namespace crv::spline
