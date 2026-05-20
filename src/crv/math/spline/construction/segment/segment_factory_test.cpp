// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

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

} // namespace
} // namespace crv::spline
