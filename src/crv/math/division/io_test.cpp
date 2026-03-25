// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "io.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <sstream>
#include <string>

namespace crv::division {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// result_t
// --------------------------------------------------------------------------------------------------------------------

struct division_result_param_t
{
    division::result_t<uint32_t> sut;
    std::string                  expected;
};

struct math_io_division_result_test_t : TestWithParam<division_result_param_t>
{};

TEST_P(math_io_division_result_test_t, result)
{
    auto const expected = GetParam().expected;

    std::ostringstream out;
    out << GetParam().sut;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

division_result_param_t const division_result_test_params[] = {
    {{.quotient = 0, .remainder = 0}, "{.quotient = 0, .remainder = 0}"},
    {{.quotient = 0, .remainder = 1}, "{.quotient = 0, .remainder = 1}"},
    {{.quotient = 1, .remainder = 0}, "{.quotient = 1, .remainder = 0}"},
    {{.quotient = 1, .remainder = 1}, "{.quotient = 1, .remainder = 1}"},

    {{.quotient = 0, .remainder = max<uint32_t>()}, "{.quotient = 0, .remainder = 4294967295}"},
    {{.quotient = max<uint32_t>(), .remainder = 0}, "{.quotient = 4294967295, .remainder = 0}"},

    {
        {
            .quotient  = max<uint32_t>(),
            .remainder = max<uint32_t>(),
        },
        "{.quotient = 4294967295, .remainder = 4294967295}",
    },
};
INSTANTIATE_TEST_SUITE_P(cases, math_io_division_result_test_t, ValuesIn(division_result_test_params));

} // namespace
} // namespace crv::division
