// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "float128.hpp"
#include <crv/math/inverse.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <sstream>

namespace crv {
namespace {

enum class format_flags_t
{
    none = 0,
    fixed = std::ios_base::fixed,
    scientific = std::ios_base::scientific,
};

struct param_t
{
    format_flags_t format_flags;
    std::streamsize precision;
    float128_t input;
    std::string expected;
};

struct float128_test_t : TestWithParam<param_t>
{
    format_flags_t format_flags = GetParam().format_flags;
    std::streamsize precision = GetParam().precision;
    float128_t input = GetParam().input;
    std::string expected = GetParam().expected;
};

TEST_P(float128_test_t, result)
{
    std::ostringstream out;
    out.flags(static_cast<std::ios_base::fmtflags>(format_flags));
    out.precision(precision);
    out << GetParam().input;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

using limits = std::numeric_limits<float128_t>;
constexpr auto inf = limits::infinity();
constexpr auto qnan = limits::quiet_NaN();

param_t const float128_test_params[] = {
    {format_flags_t::none, 6, 0.0Q, "0"},
    {format_flags_t::none, 6, -0.0Q, "-0"},
    {format_flags_t::none, 6, 1.5Q, "1.5"},
    {format_flags_t::none, 6, -1.5Q, "-1.5"},

    {format_flags_t::fixed, 2, 3.14159Q, "3.14"},
    {format_flags_t::fixed, 0, 3.99Q, "4"},
    {format_flags_t::fixed, 4, 0.0001Q, "0.0001"},

    {format_flags_t::scientific, 2, 123.456Q, "1.23e+02"},
    {format_flags_t::scientific, 3, 0.01234Q, "1.234e-02"},

    {format_flags_t::none, 6, inf, "inf"},
    {format_flags_t::none, 6, -inf, "-inf"},
    {format_flags_t::none, 6, qnan, "nan"},

    {format_flags_t::scientific, 2, limits::max(), "1.19e+4932"},
    {format_flags_t::scientific, 2, limits::lowest(), "-1.19e+4932"},
    {format_flags_t::scientific, 2, limits::min(), "3.36e-4932"},
    {format_flags_t::scientific, 2, limits::denorm_min(), "6.48e-4966"},

    {format_flags_t::none, 35, 3.14159265358979323846264338327950288Q, "3.1415926535897932384626433832795028"},

    {format_flags_t::none, 100, limits::max(),
        "1.189731495357231765085759326628007016196469052641694045529698884212163579755312392324974012848462074e+4932"},
    {format_flags_t::none, 100, limits::lowest(),
        "-1.189731495357231765085759326628007016196469052641694045529698884212163579755312392324974012848462074e+4932"},
    {format_flags_t::none, 100, limits::min(),
        "3.362103143112093506262677817321752602598079344846471240108827229808742699390728967043092706365056223e-4932"},
    {format_flags_t::none, 100, limits::denorm_min(),
        "6.47517511943802511092443895822764655249956933803468100968988438919703954012411937101767149127664994e-4966"},
};
INSTANTIATE_TEST_SUITE_P(cases, float128_test_t, ValuesIn(float128_test_params));

TEST(float128_isfinite_test_t, accepts_finite_value)
{
    EXPECT_TRUE(std::isfinite(float128_t{1}));
}

TEST(float128_isfinite_test_t, rejects_infinity)
{
    EXPECT_FALSE(std::isfinite(inf));
}

TEST(float128_isfinite_test_t, rejects_nan)
{
    EXPECT_FALSE(std::isfinite(qnan));
}

static_assert(detail::inverse::is_supported_float<float128_t>);

TEST(float128_representable_order_test_t, orders_values_across_zero)
{
    auto const denorm = limits::denorm_min();
    auto const negative_key = detail::inverse::to_key(-denorm);
    auto const zero_key = detail::inverse::to_key(float128_t{0});
    auto const positive_key = detail::inverse::to_key(denorm);

    EXPECT_EQ(negative_key + 1, zero_key);
    EXPECT_EQ(zero_key + 1, positive_key);
}

TEST(float128_representable_order_test_t, first_true_search_crosses_zero)
{
    auto const denorm = limits::denorm_min();
    EXPECT_EQ(bisect_first_true_t{}(-denorm, denorm, [](float128_t input) noexcept { return input >= float128_t{0}; }),
        float128_t{0});
}

TEST(float128_test_nextafter_t, nextafter_uses_float128_polyfill)
{
    auto const from = float128_t{1};
    auto const toward = float128_t{2};

    EXPECT_EQ(std::nextafter(from, toward), nextafterq(from, toward));
}

} // namespace
} // namespace crv
