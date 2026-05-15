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

constexpr auto pack_field = field_packer_t{};
constexpr auto unpack_field = field_unpacker_t{};

constexpr auto test_unpack(unpacked_field_t unpacked_field, field_layout_t field_layout) noexcept -> bool
{
    auto const packed_field = pack_field(unpacked_field, field_layout);
    auto const actual = unpack_field(packed_field, field_layout);
    return unpacked_field == actual;
}

constexpr auto test_unpack_positive_value_unsigned_shift() -> bool
{
    return test_unpack(
        unpacked_field_t{.mantissa = 5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = false});
}
static_assert(test_unpack_positive_value_unsigned_shift());

constexpr auto test_unpack_negative_value_unsigned_shift() -> bool
{
    return test_unpack(
        unpacked_field_t{.mantissa = -5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = false});
}
static_assert(test_unpack_negative_value_unsigned_shift());

constexpr auto test_unpack_positive_value_positive_shift() -> bool
{
    return test_unpack(
        unpacked_field_t{.mantissa = 5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = true});
}
static_assert(test_unpack_positive_value_positive_shift());

constexpr auto test_unpack_negative_value_positive_shift() -> bool
{
    return test_unpack(
        unpacked_field_t{.mantissa = -5, .shift = 3}, field_layout_t{.shift_width = 4, .is_signed = true});
}
static_assert(test_unpack_negative_value_positive_shift());

constexpr auto test_unpack_positive_value_negative_shift() -> bool
{
    return test_unpack(
        unpacked_field_t{.mantissa = 5, .shift = -3}, field_layout_t{.shift_width = 4, .is_signed = true});
}
static_assert(test_unpack_positive_value_negative_shift());

constexpr auto test_unpack_negative_value_negative_shift() -> bool
{
    return test_unpack(
        unpacked_field_t{.mantissa = -5, .shift = -3}, field_layout_t{.shift_width = 4, .is_signed = true});
}
static_assert(test_unpack_negative_value_negative_shift());

} // namespace unpacking_tests

} // namespace
} // namespace crv::spline
