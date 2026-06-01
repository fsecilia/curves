// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_packer.hpp"
#include <crv/signal_chain/spline/segment.hpp>
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

using unpacked_field_t = int_t;
using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

struct packed_field_t
{
    unpacked_field_t packed_field;
    field_layout_t field_layout;

    auto operator==(packed_field_t const&) const noexcept -> bool = default;
};
using packed_segment_t = std::array<packed_field_t, fields_per_segment>;

struct field_packer_t
{
    using packed_field_t = packed_field_t;

    constexpr auto operator()(unpacked_field_t packed_field, field_layout_t field_layout) const noexcept
        -> packed_field_t
    {
        return packed_field_t{.packed_field = packed_field, .field_layout = field_layout};
    }
};

constexpr auto pack_segment = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>{};

static_assert(pack_segment(unpacked_segment_t{7, 11, 13, 17})
    == packed_segment_t{
        packed_field_t{.packed_field = 7, .field_layout = 3},
        packed_field_t{.packed_field = 11, .field_layout = 3},
        packed_field_t{.packed_field = 13, .field_layout = 3},
        packed_field_t{.packed_field = 17, .field_layout = 5},
    });

} // namespace
} // namespace crv::spline
