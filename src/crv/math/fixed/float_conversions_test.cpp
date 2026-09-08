// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "float_conversions.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv {
namespace {

using sut_t = fixed_t<int64_t, 16>;

struct test_vector_t
{
    float_t floating_point;
    sut_t expected_fixed;
    float_t tolerance;

    friend auto operator<<(std::ostream& out, test_vector_t const& src) -> std::ostream&
    {
        return out << "{.floating_point = " << src.floating_point << ", .expected_fixed = " << src.expected_fixed
                   << ", .tolerance = " << src.tolerance << "}";
    }
};

struct fixed_test_float_conversion_t : TestWithParam<test_vector_t>
{
    float_t floating_point = GetParam().floating_point;
    sut_t expected_fixed = GetParam().expected_fixed;
    float_t tolerance = GetParam().tolerance;

    template <typename input_float_t> auto test() const noexcept -> void
    {
        auto const input_float = static_cast<input_float_t>(floating_point);

        auto const test_fixed = to_fixed<sut_t>(input_float);
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
    // exact zero and bounds
    {0.0, sut_t::literal(0), 0.0},
    {1.0, sut_t::literal(65536), 0.0},
    {-1.0, sut_t::literal(-65536), 0.0},

    // exact powers of two
    {0.5, sut_t::literal(32768), 0.0},
    {0.25, sut_t::literal(16384), 0.0},

    // standard rounding, no ties
    {1.0 + (0.4 / 65536.0), sut_t::literal(65536), 1e-5}, // rounds down
    {1.0 + (0.6 / 65536.0), sut_t::literal(65537), 1e-5}, // rounds up
    {-1.0 - (0.4 / 65536.0), sut_t::literal(-65536), 1e-5}, // rounds up towards zero
    {-1.0 - (0.6 / 65536.0), sut_t::literal(-65537), 1e-5}, // rounds down away from zero

    // positive rne tie breakers
    {0.5 / 65536.0, sut_t::literal(0), 1e-5}, // 0 (even) and 1 (odd) -> rounds to 0
    {1.5 / 65536.0, sut_t::literal(2), 1e-5}, // 1 (odd) and 2 (even) -> rounds to 2
    {2.5 / 65536.0, sut_t::literal(2), 1e-5}, // 2 (even) and 3 (odd) -> rounds to 2
    {3.5 / 65536.0, sut_t::literal(4), 1e-5}, // 3 (odd) and 4 (even) -> rounds to 4

    // negative rne tie breakers
    {-0.5 / 65536.0, sut_t::literal(0), 1e-5}, // 0 (even) and -1 (odd) -> rounds to 0
    {-1.5 / 65536.0, sut_t::literal(-2), 1e-5}, // -1 (odd) and -2 (even) -> rounds to -2
    {-2.5 / 65536.0, sut_t::literal(-2), 1e-5}, // -2 (even) and -3 (odd) -> rounds to -2
    {-3.5 / 65536.0, sut_t::literal(-4), 1e-5}, // -3 (odd) and -4 (even) -> rounds to -4
};
INSTANTIATE_TEST_SUITE_P(test_vectors, fixed_test_float_conversion_t, ValuesIn(test_vectors));

//
// signed representability
//

struct fixed_test_signed_representability_t : Test
{
    using target_t = fixed_t<int8_t, 0>;

    fixed_converter_t<target_t> converter;
};

TEST_F(fixed_test_signed_representability_t, accepts_largest_source_rounding_to_maximum)
{
    using std::nextafter;

    auto const first_unrepresentable = float64_t{127.5};
    auto const largest_representable = nextafter(first_unrepresentable, -std::numeric_limits<float64_t>::infinity());
    EXPECT_TRUE(converter.is_representable(largest_representable));
}

TEST_F(fixed_test_signed_representability_t, rejects_first_source_rounding_above_maximum)
{
    EXPECT_FALSE(converter.is_representable(float64_t{127.5}));
}

TEST_F(fixed_test_signed_representability_t, accepts_smallest_source_rounding_to_minimum)
{
    EXPECT_TRUE(converter.is_representable(float64_t{-128.5}));
}

TEST_F(fixed_test_signed_representability_t, rejects_first_source_rounding_below_minimum)
{
    using std::nextafter;

    auto const first_unrepresentable = nextafter(float64_t{-128.5}, -std::numeric_limits<float64_t>::infinity());
    EXPECT_FALSE(converter.is_representable(first_unrepresentable));
}

TEST_F(fixed_test_signed_representability_t, rejects_positive_infinity)
{
    EXPECT_FALSE(converter.is_representable(std::numeric_limits<float64_t>::infinity()));
}

TEST_F(fixed_test_signed_representability_t, rejects_negative_infinity)
{
    EXPECT_FALSE(converter.is_representable(-std::numeric_limits<float64_t>::infinity()));
}

TEST_F(fixed_test_signed_representability_t, rejects_nan)
{
    EXPECT_FALSE(converter.is_representable(std::numeric_limits<float64_t>::quiet_NaN()));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(fixed_test_signed_representability_t, conversion_asserts_unrepresentable_source)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(converter.to(float64_t{127.5})), "fixed_converter_t::to: input out of range");
}

#endif

///
/// unsigned representability
///

struct fixed_test_unsigned_representability_t : Test
{
    using target_t = fixed_t<uint8_t, 0>;

    fixed_converter_t<target_t> converter;
};

TEST_F(fixed_test_unsigned_representability_t, accepts_source_just_below_zero_when_rounding_lands_on_zero)
{
    EXPECT_TRUE(converter.is_representable(float64_t{-0.5}));
}

TEST_F(fixed_test_unsigned_representability_t, rejects_first_source_rounding_below_zero)
{
    using std::nextafter;

    auto const first_unrepresentable = nextafter(float64_t{-0.5}, -std::numeric_limits<float64_t>::infinity());
    EXPECT_FALSE(converter.is_representable(first_unrepresentable));
}

struct fixed_test_wide_conversion_signed_t : Test
{
    using target_t = fixed_t<int128_t, 0>;
};

TEST_F(fixed_test_wide_conversion_signed_t, converts_positive_value_beyond_long_long_range)
{
    using std::ldexp;

    auto const src = ldexp(float64_t{1}, 80);
    EXPECT_EQ(to_fixed<target_t>(src), target_t::literal(int128_t{1} << 80));
}

TEST_F(fixed_test_wide_conversion_signed_t, converts_negative_value_beyond_long_long_range)
{
    using std::ldexp;

    auto const src = -ldexp(float64_t{1}, 80);
    EXPECT_EQ(to_fixed<target_t>(src), target_t::literal(-(int128_t{1} << 80)));
}

struct fixed_test_wide_conversion_unsigned_t : Test
{
    using target_t = fixed_t<uint128_t, 0>;
};

TEST_F(fixed_test_wide_conversion_unsigned_t, converts_value_beyond_uint64_range)
{
    using std::ldexp;

    auto const src = ldexp(float64_t{1}, 80);
    EXPECT_EQ(to_fixed<target_t>(src), target_t::literal(uint128_t{1} << 80));
}

} // namespace
} // namespace crv
