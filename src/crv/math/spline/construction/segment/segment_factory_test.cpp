// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/math/spline/construction/segment/quantization/mantissa_quantizer.hpp>
#include <crv/math/spline/construction/segment/quantization/radix_aligner.hpp>
#include <crv/math/spline/construction/segment/quantization/shift_planner.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// ====================================================================================================================
// packing
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// segment_packer_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_packer_tests {

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

} // namespace segment_packer_tests

// ====================================================================================================================
// orchestration
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// segment_factory_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_factory_tests {

struct cubic_t
{
    int_t id;
    constexpr auto operator==(cubic_t const&) const noexcept -> bool = default;
};

struct unpacked_segment_t
{
    cubic_t cubic;
    int_t log2_width;
    constexpr auto operator==(unpacked_segment_t const&) const noexcept -> bool = default;
};

struct packed_segment_t
{
    unpacked_segment_t unpacked;
    constexpr auto operator==(packed_segment_t const&) const noexcept -> bool = default;
};

struct segment_t
{
    packed_segment_t packed_segment;

    constexpr explicit segment_t(packed_segment_t packed) noexcept : packed_segment{packed} {}
    constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
};

struct segment_quantizer_t
{
    using cubic_t = cubic_t;
    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{.cubic = cubic, .log2_width = log2_width};
    }
};

struct segment_packer_t
{
    constexpr auto operator()(unpacked_segment_t const& unpacked) const noexcept -> packed_segment_t
    {
        return packed_segment_t{.unpacked = unpacked};
    }
};

constexpr auto build_segment = segment_factory_t<segment_t, segment_quantizer_t, segment_packer_t>{};

// verify the factory correctly delegates to the quantizer, then the packer, then wraps the result
static_assert(build_segment(cubic_t{.id = 42}, 8)
    == segment_t{packed_segment_t{unpacked_segment_t{.cubic = cubic_t{.id = 42}, .log2_width = 8}}});

} // namespace segment_factory_tests

} // namespace
} // namespace crv::spline
