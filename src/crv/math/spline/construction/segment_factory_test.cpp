// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// ====================================================================================================================
// field_packer_t
// ====================================================================================================================

namespace field_packer_tests {

constexpr auto layout = field_layout_t{.shift_width = 4, .is_signed = true};
constexpr auto pack_field = field_packer_t{};

// 60-bit signed mantissa bounds
constexpr auto max_mantissa = mantissa_t{(1LL << 59) - 1};
constexpr auto min_mantissa = mantissa_t{-(1LL << 59)};

// 4-bit signed shift bounds
constexpr auto max_shift = shift_t{7};
constexpr auto min_shift = shift_t{-8};

// max mantissa, min shift
static_assert(pack_field(unpacked_field_t{.mantissa = max_mantissa, .shift = min_shift}, layout)
    == packed_field_t{0x7FFFFFFFFFFFFFF8});

// min mantissa, min shift
static_assert(pack_field(unpacked_field_t{.mantissa = min_mantissa, .shift = min_shift}, layout)
    == packed_field_t{0x8000000000000008});

// max mantissa, max shift
static_assert(pack_field(unpacked_field_t{.mantissa = max_mantissa, .shift = max_shift}, layout)
    == packed_field_t{0x7FFFFFFFFFFFFFF7});

// min mantissa, max shift
static_assert(pack_field(unpacked_field_t{.mantissa = min_mantissa, .shift = max_shift}, layout)
    == packed_field_t{0x8000000000000007});

// zero
static_assert(pack_field(unpacked_field_t{.mantissa = 0, .shift = 0}, layout) == packed_field_t{0});

} // namespace field_packer_tests

} // namespace
} // namespace crv::spline
