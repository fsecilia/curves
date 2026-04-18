// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "io.hpp"
#include <crv/test/test.hpp>
#include <sstream>

namespace crv::spline::fixed_point {
namespace {

using value_t = uint32_t;

TEST(spline_fixed_point_io, cubic_monomial)
{
    using sut_t         = cubic_monomial_t<value_t>;
    auto const sut      = sut_t{.coeffs = {2, 3, 5, 7}, .shifts = {11, 13, 17}, .final_frac_bits = 19};
    auto const expected = "{.coeffs = {2, 3, 5, 7}, .shifts = {11, 13, 17}, .final_frac_bits = 19}";

    auto out = std::ostringstream{};
    out << sut;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::spline::fixed_point
