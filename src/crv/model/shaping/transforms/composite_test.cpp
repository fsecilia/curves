// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "composite.hpp"
#include <crv/test/mock_callable.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::shaping::transforms {
namespace {

struct shaping_transforms_composite_test_t : Test
{
    using real_t = float_t;

    using mock_callable_t = mock_callable_t<real_t, real_t>;
    StrictMock<mock_callable_t> mock_inner;
    StrictMock<mock_callable_t> mock_outer;

    using callable_t = callable_t<real_t, real_t>;

    using sut_t = composite_t<callable_t, callable_t>;
    sut_t sut{.inner = {&mock_inner}, .outer = {&mock_outer}};
};

TEST_F(shaping_transforms_composite_test_t, transforms_compose_inside_out)
{
    auto const value = 3.0;
    auto const expected_inner_result = 5.0;
    auto const expected = 7.0;

    EXPECT_CALL(mock_inner, call_const(value)).WillOnce(Return(expected_inner_result));
    EXPECT_CALL(mock_outer, call_const(expected_inner_result)).WillOnce(Return(expected));

    auto const actual = sut(value);

    EXPECT_EQ(expected, actual);
}
} // namespace
} // namespace crv::shaping::transforms
