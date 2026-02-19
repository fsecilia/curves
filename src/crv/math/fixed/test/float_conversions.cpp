// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using sut_t = fixed_t<int64_t, 16>;

struct test_vector_t
{
    float_t floating_point;
    sut_t   expected_fixed;
    float_t tolerance;

    friend auto operator<<(std::ostream& out, test_vector_t const& src) -> std::ostream&
    {
        return out << "{.floating_point = " << src.floating_point << ", .expected_fixed = " << src.expected_fixed
                   << ", .tolerance = " << src.tolerance << "}";
    }
};

struct fixed_test_float_conversion_t : public TestWithParam<test_vector_t>
{
    float_t floating_point = GetParam().floating_point;
    sut_t   expected_fixed = GetParam().expected_fixed;
    float_t tolerance      = GetParam().tolerance;

    template <typename input_float_t> auto test() const noexcept -> void
    {
        auto const input_float = static_cast<input_float_t>(floating_point);

        auto const test_fixed   = to_fixed<sut_t>(input_float);
        auto const output_float = from_fixed<input_float_t>(test_fixed);

        EXPECT_EQ(expected_fixed, test_fixed);
        EXPECT_NEAR(input_float, output_float, static_cast<input_float_t>(tolerance));
    }
};

TEST_P(fixed_test_float_conversion_t, as_float32)
{
    test<float32_t>();
}

TEST_P(fixed_test_float_conversion_t, as_float64)
{
    test<float64_t>();
}

test_vector_t const test_vectors[] = {
    // 1. Exact integers
    {0.0, 0, 0},
    {1.0, 65536, 0},
    {-1.0, -65536, 0},

    // exact powers of two
    {0.5, 32768, 0},
    {0.25, 16384, 0},

    // rounding
    {1.0 + (0.4 / 65536.0), 65536, 1e-5}, // rounds down
    {1.0 + (0.6 / 65536.0), 65537, 1e-5}  // rounds up
};
INSTANTIATE_TEST_SUITE_P(test_vectors, fixed_test_float_conversion_t, ValuesIn(test_vectors));

} // namespace
} // namespace crv
