// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::shaping::stages {
namespace {

struct shaping_stages_output_test_t : Test
{
    using value_t = int_t;

    struct mock_callable_t
    {
        virtual ~mock_callable_t() = default;
        MOCK_METHOD(value_t, call, (value_t output), (const, noexcept));

        auto operator()(value_t output) const noexcept -> value_t { return call(output); }
    };
    using strict_mock_callable_t = StrictMock<mock_callable_t>;

    using sut_t = output_t<strict_mock_callable_t, strict_mock_callable_t>;
    sut_t sut{.transform = strict_mock_callable_t{}, .prev_stage = strict_mock_callable_t{}};
};

TEST_F(shaping_stages_output_test_t, call)
{
    auto const input = value_t{3};
    auto const prev_stage_output = value_t{5};
    auto const expected = value_t{7};

    EXPECT_CALL(sut.prev_stage, call(input)).WillOnce(Return(prev_stage_output));
    EXPECT_CALL(sut.transform, call(prev_stage_output)).WillOnce(Return(expected));

    auto const actual = sut(input);

    EXPECT_EQ(expected, actual);
};

} // namespace
} // namespace crv::shaping::stages
