// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "validator.hpp"
#include <crv/math/limits.hpp>
#include <crv/spline/construction/segment/field_packer.hpp>
#include <crv/spline/construction/segment/segment_packer.hpp>
#include <crv/spline/segment.hpp>
#include <crv/spline/segment_locator.hpp>
#include <crv/spline/spline.hpp>
#include <crv/spline/tangent_extension.hpp>
#include <array>

namespace crv::spline {
namespace {

struct spline_validator_test_t
{
    using x_t = fixed_t<int64_t, 0>;
    using y_t = fixed_t<int64_t, 0>;
    using unpacked_field_t = spline::unpacked_field_t<int64_t>;
    using traits_t = spline::traits_t<unpacked_field_t, y_t>;
    using packed_segment_t = traits_t::packed_segment_t;
    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using packed_field_t = traits_t::packed_field_t;
    using field_layout_t = spline::field_layout_t<packed_field_t>;
    using segment_layout_t = spline::segment_layout_t<field_layout_t>;

    static constexpr auto segment_layout = segment_layout_t{
        .intermediate = {.shift_width = 7, .is_signed = false},
        .final = {.shift_width = 7, .is_signed = true},
    };

    using field_unpacker_t = spline::field_unpacker_t<unpacked_field_t>;
    using segment_unpacker_t
        = spline::segment_unpacker_t<packed_segment_t, unpacked_segment_t, field_unpacker_t, segment_layout>;
    using segment_evaluator_t = spline::segment_evaluator_t<traits_t, x_t, y_t>;
    using segment_t = spline::segment_t<traits_t, x_t, segment_unpacker_t, segment_evaluator_t>;
    using locator_t = spline::segment_locator_t<x_t, 1>;
    using tangent_t = spline::extended_tangent_t<x_t, y_t, unpacked_field_t>;
    using spline_t = spline::spline_t<segment_t, tangent_t, locator_t>;
    using validator_t = spline::spline_validator_t<spline_t>;

    static constexpr auto pack_field = spline::field_packer_t<packed_field_t>{};
    static constexpr auto pack_segment
        = spline::segment_packer_t<packed_segment_t, unpacked_segment_t, decltype(pack_field), segment_layout>{};

    static constexpr auto make_segment(int64_t b, int64_t g0) noexcept -> segment_t
    {
        return segment_t{pack_segment(unpacked_segment_t{
            .d = {.mantissa = 0, .shift = 0},
            .c = {.mantissa = 0, .shift = 0},
            .b = {.mantissa = b, .shift = 0},
            .g0 = y_t::literal(g0),
        })};
    }

    static constexpr auto make_valid_spline() noexcept -> spline_t
    {
        auto const keys = std::array<x_t, locator_t::total_key_count>{x_t{10}, x_t{20}, x_t{20}};
        auto segments = typename spline_t::segments_t{};
        segments[0] = make_segment(1, 1);
        segments[1] = make_segment(1, 1);
        return {
            .segment_locator = locator_t{keys, x_t{20}, 2},
            .segments = segments,
            .extend_final_tangent = tangent_t{
                .slope = {.mantissa = 0, .shift = 0},
                .y0 = y_t{1},
                .x_max_delta = max<x_t>(),
            },
        };
    }
};

using fixture_t = spline_validator_test_t;
using spline_validation_error_t = spline::spline_validation_error_t;

constexpr auto validator = fixture_t::validator_t{};
constexpr auto valid_spline = fixture_t::make_valid_spline();

static_assert(validator(valid_spline) == spline_validation_result_t{});

constexpr auto invalid_locator_spline = [] {
    auto spline = fixture_t::make_valid_spline();
    auto const keys = std::array<fixture_t::x_t, fixture_t::locator_t::total_key_count>{
        fixture_t::x_t{10}, fixture_t::x_t{19}, fixture_t::x_t{20}};
    spline.segment_locator = fixture_t::locator_t{keys, fixture_t::x_t{20}, 2};
    return spline;
}();
static_assert(validator(invalid_locator_spline).error == spline_validation_error_t::locator);

constexpr auto unsafe_segment_spline = [] {
    auto spline = fixture_t::make_valid_spline();
    spline.segments[1] = fixture_t::make_segment(-(int64_t{1} << 56), max<int64_t>());
    return spline;
}();
static_assert(validator(unsafe_segment_spline)
    == spline_validation_result_t{.error = spline_validation_error_t::segment, .segment_index = 1});

constexpr auto unsafe_tangent_spline = [] {
    auto spline = fixture_t::make_valid_spline();
    spline.extend_final_tangent.slope.shift = 128;
    return spline;
}();
static_assert(validator(unsafe_tangent_spline).error == spline_validation_error_t::tangent);

constexpr auto mismatched_tangent_spline = [] {
    auto spline = fixture_t::make_valid_spline();
    spline.extend_final_tangent.y0 = fixture_t::y_t{2};
    return spline;
}();
static_assert(validator(mismatched_tangent_spline)
    == spline_validation_result_t{.error = spline_validation_error_t::tangent_anchor, .segment_index = 1});

} // namespace
} // namespace crv::spline
