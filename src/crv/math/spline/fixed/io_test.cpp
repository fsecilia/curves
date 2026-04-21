// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "io.hpp"
#include <crv/test/test.hpp>
#include <sstream>

namespace crv::spline::fixed_point {
namespace {

using out_t = fixed_t<uint64_t, 16>;
using in_t = fixed_t<uint64_t, 64>;
using coeff_t = int64_t;

struct shifter_t
{};

TEST(spline_fixed_point_io, cubic_monomial)
{
    using sut_t = cubic_monomial_t<out_t, in_t, coeff_t, shifter_t>;
    auto const sut = sut_t{.coeffs = {2, 3, 5, 7}, .shifts = {11, 13, 17}, .final_frac_bits = 19};
    auto const expected = "{.coeffs = {2, 3, 5, 7}, .shifts = {11, 13, 17}, .final_frac_bits = 19}";

    auto out = std::ostringstream{};
    out << sut;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

TEST(spline_fixed_point_io, cubic_segment)
{
    using sut_t = cubic_segment_t<cubic_monomial_t<out_t, in_t, coeff_t, shifter_t>, in_t, shifter_t>;
    auto const sut = sut_t{.monomial{.coeffs = {2, 3, 5, 7}, .shifts = {11, 13, 17}, .final_frac_bits = 19},
        .rwidth = 23,
        .rwidth_frac_bits = 29};

    auto expected = std::ostringstream{};
    expected << "{.monomial = " << sut.monomial << ", .rwidth = 23, .rwidth_frac_bits = 29}";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected.str(), actual.str());
}

} // namespace
} // namespace crv::spline::fixed_point
