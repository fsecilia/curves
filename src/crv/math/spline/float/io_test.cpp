// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "io.hpp"
#include <crv/test/test.hpp>
#include <sstream>

namespace crv::spline::floating_point {
namespace {

using real_t = float_t;

TEST(spline_floating_point_io, segment)
{
    using sut_t         = segment_t<real_t>;
    auto const sut      = sut_t{1.2, 3.4, 5.6, 7.8};
    auto const expected = "{1.2, 3.4, 5.6, 7.8}";

    auto out = std::ostringstream{};
    out << sut;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::spline::floating_point
