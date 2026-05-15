// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/spline/construction/segment_factory.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// ====================================================================================================================
// layouts
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// field_layout_t
// --------------------------------------------------------------------------------------------------------------------

//
// shift mask
//

// unsigned boundaries
static_assert(field_layout_t{0, false}.shift_mask() == 0x0ULL);
static_assert(field_layout_t{1, false}.shift_mask() == 0x1ULL);
static_assert(field_layout_t{8, false}.shift_mask() == 0xffULL);
static_assert(field_layout_t{63, false}.shift_mask() == 0x7fffffffffffffffULL);

// signed boundaries
static_assert(field_layout_t{0, true}.shift_mask() == 0x0ULL);
static_assert(field_layout_t{1, true}.shift_mask() == 0x1ULL);
static_assert(field_layout_t{8, true}.shift_mask() == 0xffULL);

//
// min shift
//

// unsigned min is always 0
static_assert(field_layout_t{0, false}.min_shift() == 0);
static_assert(field_layout_t{63, false}.min_shift() == 0);

// signed min scales by powers of two
static_assert(field_layout_t{0, true}.min_shift() == 0x0);
static_assert(field_layout_t{1, true}.min_shift() == -0x1);
static_assert(field_layout_t{8, true}.min_shift() == -0x80);
static_assert(field_layout_t{63, true}.min_shift() == -0x4000000000000000LL);

//
// max shift
//

// unsigned max matches the mask
static_assert(field_layout_t{0, false}.max_shift() == 0x0);
static_assert(field_layout_t{1, false}.max_shift() == 0x1);
static_assert(field_layout_t{8, false}.max_shift() == 0xff);

// signed max is positive half of the domain
static_assert(field_layout_t{0, true}.max_shift() == -0x1); // empty domain
static_assert(field_layout_t{1, true}.max_shift() == 0x0);
static_assert(field_layout_t{8, true}.max_shift() == 0x7f);
static_assert(field_layout_t{63, true}.max_shift() == 0x3fffffffffffffffLL);

// --------------------------------------------------------------------------------------------------------------------
// segment_layout_t
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(segment_layout_t{.intermediate = {0, false}, .final = {0, false}}.max_total_shift() == 0);

// prod pipeline, 6-bit unsigned + 7-bit signed
static_assert(segment_layout_t{.intermediate = {6, false}, .final = {7, true}}.max_total_shift() == 0x3f + 0x3f);

// asymmetric; tests intermediate and final are distinct
static_assert(segment_layout_t{.intermediate = {1, false}, .final = {8, true}}.max_total_shift() == 0x1 + 0x7f);

// ====================================================================================================================
// unpacking
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// field_unpacker_t
// --------------------------------------------------------------------------------------------------------------------

namespace field_unpacker_tests {

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

} // namespace field_unpacker_tests

// --------------------------------------------------------------------------------------------------------------------
// segment_unpacker_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_unpacker_tests {

constexpr auto intermediate_layout_shift = 5;
constexpr auto final_layout_shift = 9;
constexpr auto test_layout = segment_layout_t{
    .intermediate = field_layout_t{.shift_width = intermediate_layout_shift, .is_signed = false},
    .final = field_layout_t{.shift_width = final_layout_shift, .is_signed = true},
};

struct echoing_field_unpacker_t
{
    constexpr auto operator()(packed_field_t packed_field, field_layout_t layout) const noexcept -> unpacked_field_t
    {
        // echo input into mantissa, and fingerprint layout into shift
        return {.mantissa = static_cast<mantissa_t>(packed_field), .shift = static_cast<shift_t>(layout.shift_width)};
    }
};

constexpr auto unpacker = segment_unpacker_t<echoing_field_unpacker_t, test_layout>{};
constexpr auto packed_segment = packed_segment_t{10, 20, 30, 40};

//
// routing tests
//

// fields 0, 1, 2 get the intermediate layout
static_assert(unpacker(packed_segment, 0) == unpacked_field_t{.mantissa = 10, .shift = intermediate_layout_shift});
static_assert(unpacker(packed_segment, 1) == unpacked_field_t{.mantissa = 20, .shift = intermediate_layout_shift});
static_assert(unpacker(packed_segment, 2) == unpacked_field_t{.mantissa = 30, .shift = intermediate_layout_shift});

// field 3 gets the final layout
static_assert(unpacker(packed_segment, 3) == unpacked_field_t{.mantissa = 40, .shift = final_layout_shift});

// full segment
constexpr auto unpacked_array = unpacker(packed_segment);
static_assert(unpacked_array[0] == unpacked_field_t{.mantissa = 10, .shift = intermediate_layout_shift});
static_assert(unpacked_array[1] == unpacked_field_t{.mantissa = 20, .shift = intermediate_layout_shift});
static_assert(unpacked_array[2] == unpacked_field_t{.mantissa = 30, .shift = intermediate_layout_shift});
static_assert(unpacked_array[3] == unpacked_field_t{.mantissa = 40, .shift = final_layout_shift});

} // namespace segment_unpacker_tests

} // namespace
} // namespace crv::spline
