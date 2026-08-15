// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_packer.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using field_layout_t = int_t;
struct segment_layout_t
{
    field_layout_t intermediate;
    field_layout_t final;
};
constexpr auto segment_layout = segment_layout_t{.intermediate = 3, .final = 5};

using y_t = fixed_t<int64_t, 20>;
using unpacked_field_t = int_t;
struct unpacked_segment_t
{
    unpacked_field_t d;
    unpacked_field_t c;
    unpacked_field_t b;
    y_t g0;
};

struct packed_field_t
{
    unpacked_field_t packed_field;
    field_layout_t field_layout;

    auto operator==(packed_field_t const&) const noexcept -> bool = default;
};
struct packed_segment_t
{
    packed_field_t d;
    packed_field_t c;
    packed_field_t b;
    y_t g0;

    auto operator==(packed_segment_t const&) const noexcept -> bool = default;
};

struct field_packer_t
{
    constexpr auto operator()(unpacked_field_t packed_field, field_layout_t field_layout) const noexcept
        -> packed_field_t
    {
        return packed_field_t{.packed_field = packed_field, .field_layout = field_layout};
    }
};

constexpr auto pack_segment = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>{};
constexpr auto g0 = y_t::literal(19);

static_assert(pack_segment(unpacked_segment_t{.d = 7, .c = 11, .b = 13, .g0 = g0})
    == packed_segment_t{
        .d = {.packed_field = 7, .field_layout = 3},
        .c = {.packed_field = 11, .field_layout = 3},
        .b = {.packed_field = 13, .field_layout = 5},
        .g0 = g0,
    });

} // namespace
} // namespace crv::spline
