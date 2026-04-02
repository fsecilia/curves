// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "quantizer.hpp"
#include <crv/test/test.hpp>

namespace crv::quantizers {
namespace {

using real_t = float_t;

// ====================================================================================================================
// fixed_point_t
// ====================================================================================================================

struct test_vector_t
{
    real_t input;
    real_t expected;

    friend auto operator<<(std::ostream& out, test_vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

// common fixture
struct quantizers_fixed_point_test_t : TestWithParam<test_vector_t>
{
    real_t const input    = GetParam().input;
    real_t const expected = GetParam().expected;
};

// generic fixture
template <int frac_bits> struct quantizers_fixed_point_test_frac_bits_t : quantizers_fixed_point_test_t
{
    using sut_t = fixed_point_t<real_t, frac_bits>;
    sut_t sut{};

    auto test() -> void { EXPECT_DOUBLE_EQ(expected, sut(input)); }
};

// --------------------------------------------------------------------------------------------------------------------
// 2 frac bits
// --------------------------------------------------------------------------------------------------------------------

struct quantizers_fixed_point_test_frac_bits_2_t : quantizers_fixed_point_test_frac_bits_t<2>
{};

TEST_P(quantizers_fixed_point_test_frac_bits_2_t, test)
{
    this->test();
}

// resolution is 1/(2^2) = 0.25
test_vector_t const test_vectors_frac2[] = {
    // exact representations pass through
    {0.0, 0.0},
    {0.25, 0.25},
    {-0.5, -0.5},
    {1.75, 1.75},

    // standard rounding to nearest 0.25
    {0.1, 0.0},  // 0.4 -> 0.0 -> 0.0
    {0.2, 0.25}, // 0.8 -> 1.0 -> 0.25
    {-0.1, 0.0},
    {-0.2, -0.25},

    // positive rne halfway cases
    {0.125, 0.0}, // 0.125 * 4 = 0.5 -> rounds to 0
    {0.375, 0.5}, // 0.375 * 4 = 1.5 -> rounds to 2
    {0.625, 0.5}, // 0.625 * 4 = 2.5 -> rounds to 2
    {0.875, 1.0}, // 0.875 * 4 = 3.5 -> rounds to 4

    // negative rne halfway cases
    {-0.125, 0.0},
    {-0.375, -0.5},
};
INSTANTIATE_TEST_SUITE_P(test_vectors, quantizers_fixed_point_test_frac_bits_2_t, ValuesIn(test_vectors_frac2));

// --------------------------------------------------------------------------------------------------------------------
// 8 frac bits
// --------------------------------------------------------------------------------------------------------------------

struct quantizers_fixed_point_test_frac_bits_8_t : quantizers_fixed_point_test_frac_bits_t<8>
{};

TEST_P(quantizers_fixed_point_test_frac_bits_8_t, test)
{
    this->test();
}

// resolution is 1/(2^8) = 0.00390625
test_vector_t const test_vectors_frac8[] = {
    // exact
    {0.0, 0.0},
    {1.0, 1.0},
    {0.00390625, 0.00390625},

    // rounding
    {0.001, 0.0},
    {0.003, 0.00390625},

    // halfway cases
    {0.001953125, 0.0},      // 0.001953125*256 = 0.5
    {0.005859375, 0.0078125} // 1.5 -> 2.0
};
INSTANTIATE_TEST_SUITE_P(test_vectors, quantizers_fixed_point_test_frac_bits_8_t, ValuesIn(test_vectors_frac8));

// ====================================================================================================================
// no_op_t
// ====================================================================================================================

static_assert(no_op_t{}(-3.5) == -3.5);
static_assert(no_op_t{}(-1.0) == -1.0);
static_assert(no_op_t{}(0.0) == 0.0);
static_assert(no_op_t{}(1.0) == 1.0);
static_assert(no_op_t{}(5.5) == 5.5);

} // namespace
} // namespace crv::quantizers
