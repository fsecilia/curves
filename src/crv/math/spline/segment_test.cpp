// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/spline/construction/segment_factory.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// layouts
// --------------------------------------------------------------------------------------------------------------------

static_assert(intermediate_layout.shift_mask() == 0x3F); // 6 bits
static_assert(final_layout.shift_mask() == 0x7F); // 7 bits

// --------------------------------------------------------------------------------------------------------------------
// unpacking
// --------------------------------------------------------------------------------------------------------------------

namespace unpacking_tests {

constexpr auto test_unpack_round_trip(unpacked_field_t unpacked_field, field_layout_t field_layout) noexcept -> bool
{
    static constexpr auto pack_field = field_packer_t{};
    static constexpr auto unpack_field = field_unpacker_t{};

    auto const packed_field = pack_field(unpacked_field, field_layout);

    auto const actual = unpack_field(packed_field, field_layout);

    return unpacked_field == actual;
}

// positive value, unsigned shift
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = 5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = false}));

// negative value, unsigned shift
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = -5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = false}));

// positive value, positive shift
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = 5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// negative value, positive shift
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = -5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// positive value, negative shift
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = 5, .shift = -3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// negative value, negative shift
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = -5, .shift = -3}, field_layout_t{.shift_width = 4, .is_signed = true}));

// separate configuration
static_assert(test_unpack_round_trip(
    unpacked_field_t{.mantissa = 10, .shift = 5}, field_layout_t{.shift_width = 3, .is_signed = false}));

} // namespace unpacking_tests

} // namespace
} // namespace crv::spline
