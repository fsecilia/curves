// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/spline/segment.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using test_unpacked_field_t = unpacked_field_t<int64_t>;
using test_field_layout_t = field_layout_t<uint64_t>;
constexpr auto test_segment_layout = segment_layout_t<test_field_layout_t>{
    .intermediate = {.shift_width = 6, .is_signed = false},
    .final = {.shift_width = 7, .is_signed = true},
};

struct spline_segment_factory_test_t : Test
{
    using x_t = fixed_t<int64_t, 0>;

    struct cubic_t
    {
        int_t id;
        constexpr auto operator==(cubic_t const&) const noexcept -> bool = default;
    };

    struct unpacked_segment_t
    {
        test_unpacked_field_t b;
        cubic_t cubic;
        float_t left_endpoint_derivative;
        x_t width;
        x_t x0;
        constexpr auto operator==(unpacked_segment_t const&) const noexcept -> bool = default;
    };

    struct packed_segment_t
    {
        unpacked_segment_t unpacked;
        constexpr auto operator==(packed_segment_t const&) const noexcept -> bool = default;
    };

    struct segment_unpacker_t
    {
        static constexpr auto segment_layout = test_segment_layout;
    };

    struct segment_t
    {
        using x_t = spline_segment_factory_test_t::x_t;
        using segment_unpacker_t = spline_segment_factory_test_t::segment_unpacker_t;

        packed_segment_t packed_segment;

        constexpr explicit segment_t(packed_segment_t packed) noexcept : packed_segment{packed} {}
        constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
    };

    struct mock_segment_quantizer_t
    {
        virtual ~mock_segment_quantizer_t() = default;
        MOCK_METHOD(bool, is_g0_representable, (cubic_t const&, x_t), (const, noexcept));
        MOCK_METHOD(unpacked_segment_t, quantize, (cubic_t const&, float_t, x_t, x_t), (const, noexcept));
    };
    StrictMock<mock_segment_quantizer_t> mock_quantizer;

    struct segment_quantizer_t
    {
        using scalar_t = float_t;
        using cubic_t = spline_segment_factory_test_t::cubic_t;
        using x_t = spline_segment_factory_test_t::x_t;

        static constexpr auto max_intermediate_shift = test_segment_layout.intermediate.max_shift();

        mock_segment_quantizer_t* mock = nullptr;

        auto is_g0_representable(cubic_t const& cubic, x_t x0) const noexcept -> bool
        {
            return mock->is_g0_representable(cubic, x0);
        }

        auto operator()(cubic_t const& cubic, scalar_t left_endpoint_derivative, x_t width, x_t x0) const noexcept
            -> unpacked_segment_t
        {
            return mock->quantize(cubic, left_endpoint_derivative, width, x0);
        }
    };

    struct mock_segment_packer_t
    {
        virtual ~mock_segment_packer_t() = default;
        MOCK_METHOD(packed_segment_t, pack, (unpacked_segment_t const&), (const, noexcept));
    };
    StrictMock<mock_segment_packer_t> mock_packer;

    struct segment_packer_t
    {
        using packed_field_t = uint64_t;

        static constexpr auto segment_layout = test_segment_layout;
        mock_segment_packer_t* mock = nullptr;

        auto operator()(unpacked_segment_t const& unpacked) const noexcept -> packed_segment_t
        {
            return mock->pack(unpacked);
        }
    };

    using sut_t = spline::segment_factory_t<segment_t, segment_quantizer_t, segment_packer_t>;
    sut_t sut{{&mock_quantizer}, {&mock_packer}};

    cubic_t const cubic{42};
    float_t const left_endpoint_derivative = 1.25;
    x_t const width{8};
    x_t const x0{3};
    unpacked_segment_t const unpacked{
        .b = {.significand = 1, .shift = 0},
        .cubic = cubic,
        .left_endpoint_derivative = left_endpoint_derivative,
        .width = width,
        .x0 = x0,
    };
    packed_segment_t const packed{unpacked};
};

TEST_F(spline_segment_factory_test_t, creates_after_establishing_both_representation_preconditions)
{
    InSequence sequence;
    EXPECT_CALL(mock_quantizer, is_g0_representable(cubic, x0)).WillOnce(Return(true));
    EXPECT_CALL(mock_quantizer, quantize(cubic, left_endpoint_derivative, width, x0)).WillOnce(Return(unpacked));
    EXPECT_CALL(mock_packer, pack(unpacked)).WillOnce(Return(packed));

    auto const result = sut(cubic, left_endpoint_derivative, width, x0);

    ASSERT_TRUE(result);
    EXPECT_EQ(*result, segment_t{packed});
}

TEST_F(spline_segment_factory_test_t, rejects_unrepresentable_gain_anchor_before_quantization)
{
    EXPECT_CALL(mock_quantizer, is_g0_representable(cubic, x0)).WillOnce(Return(false));

    auto const result = sut(cubic, left_endpoint_derivative, width, x0);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(),
        (sut_t::error_t{
            .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
            .left = x0,
            .right = x0 + width,
        }));
}

TEST_F(spline_segment_factory_test_t, rejects_unencodable_final_b_before_packing)
{
    auto unencodable = unpacked;
    unencodable.b.shift = test_segment_layout.final.max_shift() + 1;

    InSequence sequence;
    EXPECT_CALL(mock_quantizer, is_g0_representable(cubic, x0)).WillOnce(Return(true));
    EXPECT_CALL(mock_quantizer, quantize(cubic, left_endpoint_derivative, width, x0)).WillOnce(Return(unencodable));

    auto const result = sut(cubic, left_endpoint_derivative, width, x0);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(),
        (sut_t::error_t{
            .reason = spline_construction_error_reason_t::left_endpoint_derivative_not_representable,
            .left = x0,
            .right = x0 + width,
        }));
}

} // namespace
} // namespace crv::spline
