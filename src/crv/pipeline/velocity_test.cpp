// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "velocity.hpp"
#include <crv/spline/pipeline_config.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <gmock/gmock.h>

namespace crv::pipeline {
namespace {

struct common_fixture_t : Test
{
    using magnitude_rsqrt_t = rsqrt_t<fixed_t<uint64_t, 62>, fixed_t<uint64_t, 0>>;
    using magnitude_t = displacement_magnitude_t<magnitude_rsqrt_t>;
};

struct displacement_magnitude_test_t : common_fixture_t
{};

TEST_F(displacement_magnitude_test_t, zero_vector_is_zero)
{
    EXPECT_EQ(magnitude_t{}(0, 0), magnitude_t::out_t{});
}

TEST_F(displacement_magnitude_test_t, three_four_five_vector)
{
    auto const actual = magnitude_t{}(3, 4);
    auto const expected = magnitude_t::out_t{5};
    EXPECT_NEAR(static_cast<double>(actual.value), static_cast<double>(expected.value), 1.0);
}

TEST_F(displacement_magnitude_test_t, axis_aligned_min_int_is_representable)
{
    auto const actual = magnitude_t{}(min<int32_t>(), 0);
    EXPECT_EQ(actual, magnitude_t::out_t{uint64_t{1} << 31});
}

TEST_F(displacement_magnitude_test_t, full_min_int_vector_stays_accurate_in_wide_result)
{
    auto const actual = magnitude_t{}(min<int32_t>(), min<int32_t>());
    auto const actual_float = std::ldexp(static_cast<double>(actual.value), -magnitude_t::out_t::frac_bits);
    auto const expected = std::sqrt(2.0) * static_cast<double>(uint64_t{1} << 31);

    EXPECT_NEAR(actual_float, expected, 1.0);
}

struct velocity_test_t : common_fixture_t
{
    using velocity_out_t = spline::prod_pipeline_config_t::x_t;

    struct mock_magnitude_t
    {
        using input_t = int32_t;
        using out_t = magnitude_t::out_t;

        virtual ~mock_magnitude_t() = default;
        MOCK_METHOD(out_t, call, (input_t, input_t), (const, noexcept));
    };
    StrictMock<mock_magnitude_t> mock_magnitude;

    struct magnitude_t
    {
        using input_t = mock_magnitude_t::input_t;
        using out_t = mock_magnitude_t::out_t;

        mock_magnitude_t* mock = nullptr;

        auto operator()(input_t x, input_t y) const noexcept -> out_t { return mock->call(x, y); }
    };

    using sut_t = velocity_t<velocity_out_t, magnitude_t>;
    sut_t const sut{magnitude_t{&mock_magnitude}};

    static_assert(std::same_as<magnitude_t::out_t::value_t, uint128_t>);
    static_assert(magnitude_t::out_t::frac_bits == 62);
    static_assert(sut_t::wide_rate_t::frac_bits == 64);
    static_assert(std::same_as<sut_t::wide_rate_t::value_t, uint128_t>);
    static_assert(sut_t::rate_t::frac_bits == 64);
    static_assert(sut_t::scale_t::int_bits == 30);
    static_assert(sut_t::scale_t::frac_bits == 34);

    // scale = 1e9/DPI. Requiring positive DPI makes the largest supported scale occur at 1 DPI, where scale=1e9.
    // unsigned Q30.34 reaches almost 2^30, so 1e9 fits while retaining 34 fractional bits
    static_assert(sut_t::scale_t{1'000'000'000} < sut_t::scale_t::literal(max<uint64_t>()));
};

TEST_F(velocity_test_t, passes_original_displacement_to_magnitude_and_scales_by_duration)
{
    EXPECT_CALL(mock_magnitude, call(3, -4)).WillOnce(Return(mock_magnitude_t::out_t{5}));

    EXPECT_EQ(sut(3, -4, sut_t::duration_t{40}, sut_t::scale_t{1000}),
        (sut_t::result_t{.value = velocity_out_t{125}, .valid = true}));
}

TEST_F(velocity_test_t, q0_64_rate_preserves_normalized_speed_near_physical_envelope_to_one_output_lsb)
{
    // 3-4-5 multiple keeps magnitude exact. At 512k DPI, scale=1953.125 and this lands near the 1414 counts/ms
    // normalized diagonal envelope without making raw rate approach the Q0.64 limit.
    EXPECT_CALL(mock_magnitude, call(434'445, 579'260)).WillOnce(Return(mock_magnitude_t::out_t{724'075}));

    auto const actual
        = sut(434'445, 579'260, sut_t::duration_t{1'000'000}, sut_t::scale_t::literal(uint64_t{33'554'432'000'000}));
    ASSERT_TRUE(actual.valid);

    auto const expected_value
        = static_cast<int64_t>(std::llround(std::ldexp(1414.208984375, velocity_out_t::frac_bits)));
    auto const error = actual.value.value >= expected_value ? actual.value.value - expected_value
                                                            : expected_value - actual.value.value;

    EXPECT_LE(error, 1);
}

TEST_F(velocity_test_t, q0_64_overflow_is_explicit_failure)
{
    EXPECT_CALL(mock_magnitude, call(1, 0)).WillOnce(Return(mock_magnitude_t::out_t{1}));

    EXPECT_FALSE(sut(1, 0, sut_t::duration_t{1}, sut_t::scale_t{1}).valid);
}

TEST_F(velocity_test_t, scaled_speed_outside_x_range_is_explicit_failure)
{
    EXPECT_CALL(mock_magnitude, call(1, 0)).WillOnce(Return(mock_magnitude_t::out_t{1}));

    EXPECT_FALSE(sut(1, 0, sut_t::duration_t{2}, sut_t::scale_t{1'000'000'000}).valid);
}

TEST_F(velocity_test_t, zero_magnitude_stays_zero)
{
    EXPECT_CALL(mock_magnitude, call(0, 0)).WillOnce(Return(mock_magnitude_t::out_t{}));

    EXPECT_EQ(sut(0, 0, sut_t::duration_t{250'000}, sut_t::scale_t{1'000'000}),
        (sut_t::result_t{.value = velocity_out_t{}, .valid = true}));
}

} // namespace
} // namespace crv::pipeline
