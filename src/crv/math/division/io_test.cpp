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
// qr_pair_t
// --------------------------------------------------------------------------------------------------------------------

struct qr_pair_param_t
{
    qr_pair_t<uint32_t> sut;
    std::string         expected;
};

struct math_division_qr_pair_io_test_t : TestWithParam<qr_pair_param_t>
{};

TEST_P(math_division_qr_pair_io_test_t, result)
{
    auto const expected = GetParam().expected;

    std::ostringstream out;
    out << GetParam().sut;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

qr_pair_param_t const qr_pair_vectors[] = {
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
INSTANTIATE_TEST_SUITE_P(qr_pair_vectors, math_division_qr_pair_io_test_t, ValuesIn(qr_pair_vectors));

} // namespace
} // namespace crv::division
