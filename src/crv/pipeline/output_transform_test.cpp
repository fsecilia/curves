// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_transform.hpp"
#include <crv/spline/pipeline_config.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline {
namespace {

struct output_transform_test_t : Test
{
    using gain_t = spline::prod_pipeline_config_t::y_t;
    using sut_t = output_transform_t<gain_t>;
    using out_t = sut_t::out_t;

    static constexpr auto one = sut_t::coefficient_t{1};
    static constexpr auto zero = sut_t::coefficient_t{};

    static_assert(sut_t::coefficient_t::int_bits == 10);
    static_assert(sut_t::coefficient_t::frac_bits == 53);
    static_assert(sut_t::transform_t::int_bits == 31);
    static_assert(sut_t::transform_t::frac_bits == 32);
    static_assert(sut_t::out_t::frac_bits == gain_t::frac_bits);
    static_assert(sut_t::input_limit == (int32_t{1} << 20));

    // At 128k DPI, 1000 in/s, and 125 Hz, one axis reaches 1,024,000 counts/report. The |axis| < 2^20 envelope
    // leaves 24,576 counts of headroom. Matrix terms can reach 1000, so both row dot products stay below 2^31.
    // Q10.53 coefficient rounding plus Q31.32 narrowing stays below about 2.33e-7 output counts at gain 1000.

    sut_t sut{};
};

TEST_F(output_transform_test_t, default_transform_is_identity)
{
    auto const result = sut(17, -29, gain_t{1});

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.x, out_t{17});
    EXPECT_EQ(result.y, out_t{-29});
}

TEST_F(output_transform_test_t, precomposed_rotation_and_anisotropy_form_two_rows)
{
    sut.matrix = {{
        {zero, -one},
        {sut_t::coefficient_t{2}, zero},
    }};

    auto const result = sut(3, 4, gain_t{1});

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.x, out_t{-4});
    EXPECT_EQ(result.y, out_t{6});
}

TEST_F(output_transform_test_t, scalar_gain_is_applied_after_linear_transform)
{
    sut.matrix = {{
        {zero, -one},
        {sut_t::coefficient_t{2}, zero},
    }};
    auto const gain = gain_t::literal(int64_t{3} << (gain_t::frac_bits - 1)); // 1.5

    auto const result = sut(2, 4, gain);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.x, out_t{-6});
    EXPECT_EQ(result.y, out_t{6});
}

TEST_F(output_transform_test_t, supported_input_envelope_is_checked_before_transform)
{
    EXPECT_TRUE(sut(sut_t::input_limit - 1, -(sut_t::input_limit - 1), gain_t{1}).valid);
    EXPECT_FALSE(sut(sut_t::input_limit, 0, gain_t{1}).valid);
    EXPECT_FALSE(sut(-sut_t::input_limit, 0, gain_t{1}).valid);
}

TEST_F(output_transform_test_t, envelope_and_coefficient_bounds_fit_q31_32_intermediate)
{
    auto const max_input = int64_t{sut_t::input_limit - 1};
    auto const worst_row = int128_t{2} * max_input * 1000;
    auto const transform_integer_limit = int128_t{1} << sut_t::transform_t::int_bits;

    EXPECT_LT(worst_row, transform_integer_limit);
}

TEST_F(output_transform_test_t, pathological_gain_anisotropy_case_keeps_error_in_low_e7_range)
{
    // 45-degree rotation at anisotropy 1000, rounded to Q10.53
    constexpr auto inv_sqrt2 = sut_t::coefficient_t::literal(6369051672525773LL);
    constexpr auto anisotropic_inv_sqrt2 = sut_t::coefficient_t::literal(6369051672525772565LL);
    sut.matrix = {{
        {inv_sqrt2, -inv_sqrt2},
        {anisotropic_inv_sqrt2, anisotropic_inv_sqrt2},
    }};
    auto const input = sut_t::input_limit - 1;

    auto const result = sut(input, input, gain_t{1000});
    ASSERT_TRUE(result.valid);

    // Q53 reference for (2^20 - 1) * sqrt(2) * 1000 * 1000
    constexpr auto expected_raw = (int128_t{0x2b28886d} << 64) | int128_t{0x66abc7cf358dbe1aULL};
    auto const error = result.y.value >= expected_raw ? result.y.value - expected_raw : expected_raw - result.y.value;

    // 997,343,718 Q53 ulps ~= 1.1073e-7 output counts
    EXPECT_LT(error, int128_t{1'100'000'000});
}

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct output_transform_death_test_t : output_transform_test_t
{};

TEST_F(output_transform_death_test_t, matrix_component_bounds_are_debug_assertions)
{
    sut.matrix[1][0] = sut_t::coefficient_t{1001};

    EXPECT_DEBUG_DEATH((void)sut(0, 0, gain_t{1}), "anisotropy row component outside range");
}

TEST_F(output_transform_death_test_t, rotation_row_norm)
{
    sut.matrix[0] = {zero, zero};
    EXPECT_DEBUG_DEATH((void)sut(0, 0, gain_t{1}), "rotation row norm must be one");

    sut.matrix[0] = {one, one};
    EXPECT_DEBUG_DEATH((void)sut(0, 0, gain_t{1}), "rotation row norm must be one");
}

TEST_F(output_transform_death_test_t, row_orthogonality)
{
    sut.matrix[1] = {one, one};

    EXPECT_DEBUG_DEATH((void)sut(0, 0, gain_t{1}), "matrix rows must be orthogonal");
}

TEST_F(output_transform_death_test_t, anisotropy_row_norm)
{
    constexpr auto inv_sqrt2 = sut_t::coefficient_t::literal(6369051672525773LL);
    sut.matrix = {{
        {inv_sqrt2, -inv_sqrt2},
        {sut_t::coefficient_t{800}, sut_t::coefficient_t{800}},
    }};

    EXPECT_DEBUG_DEATH((void)sut(0, 0, gain_t{1}), "anisotropy outside supported range");
}

TEST_F(output_transform_death_test_t, positive_anisotropy)
{
    sut.matrix[1] = {zero, -one};

    EXPECT_DEBUG_DEATH((void)sut(0, 0, gain_t{1}), "anisotropy must be positive");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::pipeline
