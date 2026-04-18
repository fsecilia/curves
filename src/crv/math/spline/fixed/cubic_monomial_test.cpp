// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cubic_monomial.hpp"
#include <crv/math/limits.hpp>
#include <crv/math/spline/fixed/io.hpp>
#include <crv/test/test.hpp>
#include <ostream>

namespace crv::spline::fixed_point {
namespace {

using value_t = int32_t;
using sut_t   = cubic_monomial_t<value_t>;

constexpr auto in_frac_bits  = 16;
constexpr auto out_frac_bits = 16;

using in_t  = fixed_t<value_t, in_frac_bits>;
using out_t = fixed_t<value_t, out_frac_bits>;

// 1 << 16 represents 1.0 in our t domain
constexpr auto t_one  = value_t{1 << in_frac_bits};
constexpr auto t_half = value_t{1 << (in_frac_bits - 1)};

struct param_t
{
    sut_t   sut;
    value_t t_value;        // literal integer for in_t t
    value_t expected_value; // literal integer we expect out

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        return out << "{.sut = " << src.sut << ", .t_value = " << src.t_value
                   << ", expected_value = " << src.expected_value << " }";
    }
};

struct spline_fixed_point_segment_parameterized_test_t : testing::TestWithParam<param_t>
{
    sut_t const&  sut            = GetParam().sut;
    value_t const t_value        = GetParam().t_value;
    value_t const expected_value = GetParam().expected_value;
};

TEST_P(spline_fixed_point_segment_parameterized_test_t, expected_result)
{
    auto const t_fixed = in_t::literal(t_value);

    // Evaluate and extract the literal value to compare against our hand-calculated expectation
    auto const actual_value = sut.template evaluate<out_frac_bits>(t_fixed).value;

    EXPECT_EQ(expected_value, actual_value);
};

param_t const params[] = {
    // all zeros
    {.sut = {{0, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, .t_value = t_half, .expected_value = 0},

    // constant evaluation
    {.sut = {{0, 0, 0, 1024}, {0, 0, 0}, out_frac_bits}, .t_value = t_half, .expected_value = 1024},

    // relative shift alignment
    {.sut = {{2, 4, 0, 0}, {16, 0, 0}, out_frac_bits}, .t_value = t_half, .expected_value = 1},

    // negative coefficients
    {.sut = {{-16, 0, 0, 0}, {0, 0, 0}, out_frac_bits}, .t_value = t_half, .expected_value = -2},
    {.sut = {{0, 0, 0, -100}, {0, 0, 0}, out_frac_bits}, .t_value = t_half, .expected_value = -100},

    // final frac bits
    {.sut = {{0, 0, 0, 4096}, {0, 0, 0}, 18}, .t_value = 0, .expected_value = 1024},
    {.sut = {{0, 0, 0, 256}, {0, 0, 0}, 14}, .t_value = 0, .expected_value = 1024},
};
INSTANTIATE_TEST_SUITE_P(params, spline_fixed_point_segment_parameterized_test_t, ValuesIn(params));

// baseline truncating shifter
struct truncating_shifter_t
{
    template <typename val_t> [[nodiscard]] constexpr auto shr(val_t val, int_t bits) const noexcept -> val_t
    {
        return val >> bits;
    }

    template <typename val_t> [[nodiscard]] constexpr auto shift(val_t val, int_t bits) const noexcept -> val_t
    {
        return (bits < 0) ? (val >> -bits) : (val << bits);
    }
};

// perturbed shifter that injects a deterministic offset to prove it was called
struct perturbed_shifter_t
{
    template <typename val_t> [[nodiscard]] constexpr auto shr(val_t val, int_t bits) const noexcept -> val_t
    {
        // truncate, but add 1 to the result of every intermediate right-shift
        return (val >> bits) + 1;
    }

    template <typename val_t> [[nodiscard]] constexpr auto shift(val_t val, int_t bits) const noexcept -> val_t
    {
        // don't perturb the final absolute shift, just the relative intermediate ones
        return (bits < 0) ? (val >> -bits) : (val << bits);
    }
};

TEST(spline_fixed_point_segment_test_t, negative_early_coefficients_with_truncation)
{
    sut_t const sut     = {{-4096, -2048, 0, 0}, {0, 0, 0}, out_frac_bits};
    auto const  t_fixed = in_t::literal(16384);

    auto const actual_value = sut.template evaluate<out_frac_bits>(t_fixed, truncating_shifter_t{}).value;

    EXPECT_EQ(-192, actual_value);
}

TEST(spline_fixed_point_segment_test_t, evaluates_using_injected_shifter)
{
    sut_t const sut     = {{0, 0, 0, 0}, {16, 16, 16}, out_frac_bits};
    auto const  t_fixed = in_t::literal(0);

    auto const actual_value = sut.evaluate<out_frac_bits>(t_fixed, perturbed_shifter_t{}).value;

    // this would be 0 with the truncating shifter
    EXPECT_EQ(1, actual_value);
}

TEST(spline_fixed_point_segment_test_t, evaluates_at_compile_time)
{
    constexpr sut_t sut     = {{1024, 2048, 4096, 8192}, {0, 0, 0}, out_frac_bits};
    constexpr auto  t_fixed = in_t::literal(t_half);

    constexpr auto actual = sut.evaluate<out_frac_bits>(t_fixed, truncating_shifter_t{});

    static_assert(actual.value == 10880, "Constexpr evaluation failed or mismatched!");
}

TEST(spline_fixed_point_segment_test_t, asymmetric_output_domain)
{
    constexpr auto other_out_frac_bits = 8;

    sut_t const sut = {{0, 0, 0, 16384}, {0, 0, 0}, 16};

    auto const actual_value = sut.evaluate<other_out_frac_bits, in_frac_bits>(in_t::literal(0)).value;

    EXPECT_EQ(64, actual_value);
}

TEST(spline_fixed_point_segment_test_t, t_equals_one_inclusive_upper_bound)
{
    sut_t const sut     = {{1024, 2048, 4096, 8192}, {0, 0, 0}, out_frac_bits};
    auto const  t_fixed = in_t::literal(t_one);

    auto const actual_value = sut.evaluate<out_frac_bits>(t_fixed, truncating_shifter_t{}).value;

    EXPECT_EQ(15360, actual_value);
}

} // namespace
} // namespace crv::spline::fixed_point
