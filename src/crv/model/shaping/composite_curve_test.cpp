// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "composite_curve.hpp"
#include <crv/test/mock_callable.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::shaping {
namespace {

struct shaping_composite_curve_test_t : Test
{
    using real_t = float_t;

    using mock_callable_t = mock_callable_t<real_t, real_t>;
    StrictMock<mock_callable_t> mock_in;
    StrictMock<mock_callable_t> mock_curve;
    StrictMock<mock_callable_t> mock_out;

    using callable_t = callable_t<real_t, real_t>;

    using sut_t = composite_curve_t<callable_t, callable_t, callable_t>;
    sut_t sut{.in = {&mock_in}, .curve = {&mock_curve}, .out = {&mock_out}};
};

TEST_F(shaping_composite_curve_test_t, transforms_compose_inside_out)
{
    auto const value = 3.0;
    auto const expected_in_result = 5.0;
    auto const expected_curve_result = 7.0;
    auto const expected = 11.0;

    EXPECT_CALL(mock_in, call_const(value)).WillOnce(Return(expected_in_result));
    EXPECT_CALL(mock_curve, call_const(expected_in_result)).WillOnce(Return(expected_curve_result));
    EXPECT_CALL(mock_out, call_const(expected_curve_result)).WillOnce(Return(expected));

    auto const actual = sut(value);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::shaping
