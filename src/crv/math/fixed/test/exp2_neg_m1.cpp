// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/fixed/exp2_neg_m1.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using in_t  = exp2_neg_m1_q64_to_q1_63_t::in_t;
using out_t = exp2_neg_m1_q64_to_q1_63_t::out_t;

struct vector_t
{
    in_t  input;
    out_t expected;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input.value << ", .expected = " << src.expected.value << "}";
    }
};

struct exp2_neg_m1_q64_to_q1_63_t_test_t : TestWithParam<vector_t>
{
    in_t const&  input    = GetParam().input;
    out_t const& expected = GetParam().expected;

    using sut_t = exp2_neg_m1_q64_to_q1_63_t;
    sut_t sut{};
};

TEST_P(exp2_neg_m1_q64_to_q1_63_t_test_t, eval)
{
    auto const actual = sut.eval(input);

    EXPECT_EQ(expected, actual);
}

// clang-format off
vector_t const vectors[] = {
    vector_t{in_t{0x0000'0000'0000'0000}, out_t{ 0x0000'0000'0000'0000}},
    vector_t{in_t{0x0000'0000'0000'0001}, out_t{ 0x0000'0000'0000'0000}},

    vector_t{in_t{0x3fff'ffff'ffff'ffff}, out_t{-0x145D'819A'945E'FA86}},
    vector_t{in_t{0x4000'0000'0000'0000}, out_t{-0x145D'819A'945E'FA86}}, // 2^-0.25 - 1 = -0.159103585
    vector_t{in_t{0x4000'0000'0000'0001}, out_t{-0x145D'819A'945E'FA87}},

    vector_t{in_t{0x7fff'ffff'ffff'ffff}, out_t{-0x257D'8666'0308'44DA}},
    vector_t{in_t{0x8000'0000'0000'0000}, out_t{-0x257D'8666'0308'44D9}}, // 2^-0.5 - 1 = -0.292893219
    vector_t{in_t{0x8000'0000'0000'0001}, out_t{-0x257D'8666'0308'44DA}},

    vector_t{in_t{0xafff'ffff'ffff'ffff}, out_t{-0x3085'66CF'B74F'8E69}},
    vector_t{in_t{0xb000'0000'0000'0000}, out_t{-0x3085'66CF'B74F'8E68}}, // 2^-0.75 - 1 = -0.405396442
    vector_t{in_t{0xb000'0000'0000'0001}, out_t{-0x3085'66CF'B74F'8E69}},

    vector_t{in_t{0xffff'ffff'ffff'fffe}, out_t{-0x3FFF'FFFF'FFAC'C7F7}},
    vector_t{in_t{0xffff'ffff'ffff'ffff}, out_t{-0x3FFF'FFFF'FFAC'C7F7}}, // 2^-1 - 1 = -0.5
};
INSTANTIATE_TEST_SUITE_P(vectors, exp2_neg_m1_q64_to_q1_63_t_test_t, ValuesIn(vectors));
// clang-format on

} // namespace
} // namespace crv
