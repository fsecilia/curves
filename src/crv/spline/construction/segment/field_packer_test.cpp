// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "field_packer.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using traits_t = traits_t<unpacked_field_t<int_t>, fixed_t<int64_t, 20>>;

using packed_field_t = traits_t::packed_field_t;
using unpacked_field_t = traits_t::unpacked_field_t;
using significand_t = traits_t::significand_t;

using field_layout_t = field_layout_t<packed_field_t>;
using field_packer_t = field_packer_t<packed_field_t>;

constexpr auto layout = field_layout_t{.shift_width = 4, .is_signed = true};
constexpr auto pack_field = field_packer_t{};

// 60-bit signed significand bounds
constexpr auto max_significand = significand_t{(1LL << 59) - 1};
constexpr auto min_significand = significand_t{-(1LL << 59)};

// 4-bit signed shift bounds
constexpr auto max_shift = int_t{7};
constexpr auto min_shift = int_t{-8};

// max significand, min shift
static_assert(pack_field(unpacked_field_t{.significand = max_significand, .shift = min_shift}, layout)
    == packed_field_t{0x7FFFFFFFFFFFFFF8});

// min significand, min shift
static_assert(pack_field(unpacked_field_t{.significand = min_significand, .shift = min_shift}, layout)
    == packed_field_t{0x8000000000000008});

// max significand, max shift
static_assert(pack_field(unpacked_field_t{.significand = max_significand, .shift = max_shift}, layout)
    == packed_field_t{0x7FFFFFFFFFFFFFF7});

// min significand, max shift
static_assert(pack_field(unpacked_field_t{.significand = min_significand, .shift = max_shift}, layout)
    == packed_field_t{0x8000000000000007});

// zero
static_assert(pack_field(unpacked_field_t{.significand = 0, .shift = 0}, layout) == packed_field_t{0});

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct field_packer_test_t : Test
{};

TEST_F(field_packer_test_t, asserts_unencodable_field)
{
    auto const unencodable = unpacked_field_t{.significand = max_significand + 1, .shift = 0};
    EXPECT_DEBUG_DEATH(static_cast<void>(pack_field(unencodable, layout)), "unpacked field is not encodable");
}

#endif

} // namespace
} // namespace crv::spline
