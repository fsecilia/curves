// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic field packing
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/segment.hpp>

namespace crv::spline {

/// tightly packs individual fields into specific bit ranges of packed_field_t
template <typename t_packed_field_t> struct field_packer_t
{
    using packed_field_t = t_packed_field_t;

    template <typename unpacked_field_t, typename field_layout_t>
    constexpr auto operator()(unpacked_field_t unpacked_field, field_layout_t layout) const noexcept -> packed_field_t
    {
        auto const packed_mantissa = static_cast<packed_field_t>(unpacked_field.mantissa) << layout.shift_width;
        auto const packed_shift = unpacked_field.shift & layout.shift_mask();

        auto const packed_field = packed_field_t{packed_mantissa | packed_shift};
        assert(field_unpacker_t<unpacked_field_t>{}(packed_field, layout) == unpacked_field);

        return packed_field;
    }
};

} // namespace crv::spline
