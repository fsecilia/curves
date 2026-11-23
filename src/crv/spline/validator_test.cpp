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
#include <crv/test/test.hpp>
#include <array>
#include <gmock/gmock.h>

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
    using locator_validator_t = locator_t::validator_t;
    using segment_validator_t = segment_t::validator_t;
    using validator_t = spline::spline_validator_t<spline_t, locator_validator_t, segment_validator_t>;

    static constexpr auto pack_field = spline::field_packer_t<packed_field_t>{};
    static constexpr auto pack_segment
        = spline::segment_packer_t<packed_segment_t, unpacked_segment_t, decltype(pack_field), segment_layout>{};

    static constexpr auto make_segment(int64_t b, int64_t g0) noexcept -> segment_t
    {
        return segment_t{pack_segment(unpacked_segment_t{
            .d = {.significand = 0, .shift = 0},
            .c = {.significand = 0, .shift = 0},
            .b = {.significand = b, .shift = 0},
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
                .slope = {.significand = 0, .shift = 0},
                .y0 = y_t{1},
                .x_max_delta = max<x_t>(),
            },
        };
    }
};

using fixture_t = spline_validator_test_t;
using spline_validation_error_t = spline::spline_validation_error_t;

constexpr auto validator = fixture_t::validator_t{.validate_segment_locator = {}, .validate_segment = {}};
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

struct injected_locator_validator_t
{
    bool accepted;

    constexpr auto operator()(fixture_t::locator_t const&) const noexcept -> bool { return accepted; }
};

constexpr auto rejecting_locator_validator
    = spline_validator_t<fixture_t::spline_t, injected_locator_validator_t, fixture_t::segment_validator_t>{
        .validate_segment_locator = {.accepted = false},
        .validate_segment = {},
    };
static_assert(rejecting_locator_validator(valid_spline).error == spline_validation_error_t::locator);

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

struct component_validated_tangent_t
{
    fixture_t::y_t y0{};
    bool accepted = true;

    struct validator_t
    {
        constexpr auto operator()(component_validated_tangent_t const& tangent) const noexcept -> bool
        {
            return tangent.accepted;
        }
    };
};

using component_validated_spline_t
    = spline::spline_t<fixture_t::segment_t, component_validated_tangent_t, fixture_t::locator_t>;

constexpr auto component_rejected_tangent_spline = [] {
    auto const source = fixture_t::make_valid_spline();
    return component_validated_spline_t{
        .segment_locator = source.segment_locator,
        .segments = source.segments,
        .extend_final_tangent = {.y0 = source.extend_final_tangent.y0, .accepted = false},
    };
}();
static_assert(
    spline_validator_t<component_validated_spline_t, fixture_t::locator_validator_t, fixture_t::segment_validator_t>{
        .validate_segment_locator = {}, .validate_segment = {}}(component_rejected_tangent_spline)
        .error
    == spline_validation_error_t::tangent);

constexpr auto mismatched_tangent_spline = [] {
    auto spline = fixture_t::make_valid_spline();
    spline.extend_final_tangent.y0 = fixture_t::y_t{2};
    return spline;
}();
static_assert(validator(mismatched_tangent_spline)
    == spline_validation_result_t{.error = spline_validation_error_t::tangent_anchor, .segment_index = 1});

struct spline_validator_interaction_test_t : Test
{
    using x_t = fixture_t::x_t;
    using y_t = fixture_t::y_t;
    using locator_t = fixture_t::locator_t;

    struct mock_segment_t
    {
        virtual ~mock_segment_t() = default;
        MOCK_METHOD(y_t, evaluate, (int_t id, x_t x, x_t x0), (const, noexcept));
    };
    StrictMock<mock_segment_t> mock_segment;

    struct segment_t
    {
        using x_t = spline_validator_interaction_test_t::x_t;
        using y_t = spline_validator_interaction_test_t::y_t;

        int_t id;
        mock_segment_t* mock;

        auto operator()(x_t x, x_t x0) const noexcept -> y_t { return mock->evaluate(id, x, x0); }
    };

    struct mock_tangent_validator_t
    {
        virtual ~mock_tangent_validator_t() = default;
        MOCK_METHOD(bool, call, (y_t anchor), (const, noexcept));
    };
    StrictMock<mock_tangent_validator_t> mock_tangent_validator;

    struct tangent_t
    {
        y_t y0;
        mock_tangent_validator_t* mock;

        struct validator_t
        {
            auto operator()(tangent_t const& tangent) const noexcept -> bool { return tangent.mock->call(tangent.y0); }
        };
    };

    struct mock_locator_validator_t
    {
        virtual ~mock_locator_validator_t() = default;
        MOCK_METHOD(bool, call, (locator_t const& locator), (const, noexcept));
    };
    StrictMock<mock_locator_validator_t> mock_locator_validator;

    struct locator_validator_t
    {
        mock_locator_validator_t* mock;
        auto operator()(locator_t const& locator) const noexcept -> bool { return mock->call(locator); }
    };

    struct mock_segment_validator_t
    {
        virtual ~mock_segment_validator_t() = default;
        MOCK_METHOD(bool, call, (segment_t const& segment, x_t width, x_t origin), (const, noexcept));
    };
    StrictMock<mock_segment_validator_t> mock_segment_validator;

    struct segment_validator_t
    {
        mock_segment_validator_t* mock;
        auto operator()(segment_t const& segment, x_t width, x_t origin) const noexcept -> bool
        {
            return mock->call(segment, width, origin);
        }
    };

    using spline_t = spline::spline_t<segment_t, tangent_t, locator_t>;
    using sut_t = spline_validator_t<spline_t, locator_validator_t, segment_validator_t>;

    x_t const knot{7};
    x_t const end{20};
    y_t const anchor{5};
    spline_t spline{
        .segment_locator = locator_t{std::array<x_t, locator_t::total_key_count>{knot, end, end}, end, 2},
        .segments = {segment_t{11, &mock_segment}, segment_t{19, &mock_segment}, segment_t{29, &mock_segment},
            segment_t{31, &mock_segment}},
        .extend_final_tangent = {.y0 = anchor, .mock = &mock_tangent_validator},
    };
    sut_t sut{.validate_segment_locator = {&mock_locator_validator}, .validate_segment = {&mock_segment_validator}};
};

TEST_F(spline_validator_interaction_test_t, rejected_locator_skips_all_later_validation)
{
    EXPECT_CALL(mock_locator_validator, call(Ref(spline.segment_locator))).WillOnce(Return(false));

    EXPECT_EQ(sut(spline), (spline_validation_result_t{.error = spline_validation_error_t::locator}));
}

TEST_F(spline_validator_interaction_test_t, accepts_only_active_segments_before_tangent_and_endpoint)
{
    InSequence sequence;
    EXPECT_CALL(mock_locator_validator, call(Ref(spline.segment_locator))).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[0]), knot, x_t{0})).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[1]), end - knot, knot)).WillOnce(Return(true));
    EXPECT_CALL(mock_tangent_validator, call(anchor)).WillOnce(Return(true));
    EXPECT_CALL(mock_segment, evaluate(spline.segments[1].id, end, knot)).WillOnce(Return(anchor));

    EXPECT_EQ(sut(spline), spline_validation_result_t{});
}

TEST_F(spline_validator_interaction_test_t, rejected_tangent_skips_endpoint)
{
    InSequence sequence;
    EXPECT_CALL(mock_locator_validator, call(Ref(spline.segment_locator))).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[0]), knot, x_t{0})).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[1]), end - knot, knot)).WillOnce(Return(true));
    EXPECT_CALL(mock_tangent_validator, call(anchor)).WillOnce(Return(false));

    EXPECT_EQ(sut(spline), (spline_validation_result_t{.error = spline_validation_error_t::tangent}));
}

TEST_F(spline_validator_interaction_test_t, rejected_first_segment_skips_remaining_segments_and_tangent)
{
    InSequence sequence;
    EXPECT_CALL(mock_locator_validator, call(Ref(spline.segment_locator))).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[0]), knot, x_t{0})).WillOnce(Return(false));

    EXPECT_EQ(
        sut(spline), (spline_validation_result_t{.error = spline_validation_error_t::segment, .segment_index = 0}));
}

TEST_F(spline_validator_interaction_test_t, rejected_final_segment_skips_tangent_and_endpoint)
{
    InSequence sequence;
    EXPECT_CALL(mock_locator_validator, call(Ref(spline.segment_locator))).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[0]), knot, x_t{0})).WillOnce(Return(true));
    EXPECT_CALL(mock_segment_validator, call(Ref(spline.segments[1]), end - knot, knot)).WillOnce(Return(false));

    EXPECT_EQ(
        sut(spline), (spline_validation_result_t{.error = spline_validation_error_t::segment, .segment_index = 1}));
}

} // namespace
} // namespace crv::spline
