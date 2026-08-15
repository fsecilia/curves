// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

struct field_layout_t
{
    int_t shift_width;
    bool is_signed;

    constexpr auto max_shift() const noexcept -> int_t { return (1 << shift_width) - 1; }

    constexpr auto operator==(field_layout_t const&) const noexcept -> bool = default;
};

struct segment_layout_t
{
    field_layout_t intermediate;
    field_layout_t final;

    constexpr auto operator==(segment_layout_t const&) const noexcept -> bool = default;
};

constexpr auto segment_layout = segment_layout_t{
    .intermediate = {.shift_width = 6, .is_signed = false},
    .final = {.shift_width = 7, .is_signed = true},
};

struct cubic_t
{
    int_t id;
    constexpr auto operator==(cubic_t const&) const noexcept -> bool = default;
};

struct unpacked_segment_t
{
    cubic_t cubic;
    int_t width;
    int_t x0;
    constexpr auto operator==(unpacked_segment_t const&) const noexcept -> bool = default;
};

struct packed_segment_t
{
    unpacked_segment_t unpacked;
    constexpr auto operator==(packed_segment_t const&) const noexcept -> bool = default;
};

struct segment_unpacker_t
{
    static constexpr auto segment_layout = spline::segment_layout;
};

struct segment_t
{
    using x_t = int_t;
    using segment_unpacker_t = segment_unpacker_t;

    packed_segment_t packed_segment;

    constexpr explicit segment_t(packed_segment_t packed) noexcept : packed_segment{packed} {}
    constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
};

struct segment_quantizer_t
{
    using cubic_t = cubic_t;
    using x_t = int_t;

    static constexpr auto max_intermediate_shift = spline::segment_layout.intermediate.max_shift();

    constexpr auto operator()(cubic_t const& cubic, x_t width, x_t x0) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{.cubic = cubic, .width = width, .x0 = x0};
    }
};

struct segment_packer_t
{
    static constexpr auto segment_layout = spline::segment_layout;
    constexpr auto operator()(unpacked_segment_t const& unpacked) const noexcept -> packed_segment_t
    {
        return packed_segment_t{.unpacked = unpacked};
    }
};

constexpr auto build_segment = segment_factory_t<segment_t, segment_quantizer_t, segment_packer_t>{};

// verify the factory correctly delegates to the quantizer, then the packer, then wraps the result
static_assert(build_segment(cubic_t{.id = 42}, 8, 3)
    == segment_t{packed_segment_t{unpacked_segment_t{.cubic = cubic_t{.id = 42}, .width = 8, .x0 = 3}}});

} // namespace
} // namespace crv::spline
