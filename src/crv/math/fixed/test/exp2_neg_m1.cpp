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
    vector_t{in_t{0x0000'0000'0000'0000}, out_t{ 0x0000'0000'0000'0000}}, // 2^0 - 1 = 0
    vector_t{in_t{0x0000'0000'0000'0001}, out_t{ 0x0000'0000'0000'0000}},

    vector_t{in_t{0x3fff'ffff'ffff'ffff}, out_t{-0x145d'819a'9458'd280}},
    vector_t{in_t{0x4000'0000'0000'0000}, out_t{-0x145d'819a'9458'd280}}, // 2^-0.25 - 1 = -0.159103585
    vector_t{in_t{0x4000'0000'0000'0001}, out_t{-0x145d'819a'9458'd281}},

    vector_t{in_t{0x7fff'ffff'ffff'ffff}, out_t{-0x257d'8666'030d'c49f}},
    vector_t{in_t{0x8000'0000'0000'0000}, out_t{-0x257d'8666'030d'c49f}}, // 2^-0.5 - 1 = -0.292893219
    vector_t{in_t{0x8000'0000'0000'0001}, out_t{-0x257d'8666'030d'c4a0}},

    vector_t{in_t{0xbfff'ffff'ffff'ffff}, out_t{-0x33e4'07d7'397c'8cf3}},
    vector_t{in_t{0xc000'0000'0000'0000}, out_t{-0x33e4'07d7'397c'8cf3}}, // 2^-0.75 - 1 = 0.405396442
    vector_t{in_t{0xc000'0000'0000'0001}, out_t{-0x33e4'07d7'397c'8cf3}},

    vector_t{in_t{0xffff'ffff'ffff'fffe}, out_t{-0x4000'0000'0000'0000}},
    vector_t{in_t{0xffff'ffff'ffff'ffff}, out_t{-0x4000'0000'0000'0001}}, // 2^-1 - 1 = -0.5
};
INSTANTIATE_TEST_SUITE_P(vectors, exp2_neg_m1_q64_to_q1_63_t_test_t, ValuesIn(vectors));
// clang-format on

} // namespace
} // namespace crv
