// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "weight_function.hpp"
#include <crv/test/test.hpp>

namespace crv::weight_functions {
namespace {

using real_t = float_t;

// --------------------------------------------------------------------------------------------------------------------
// uniform_t
// --------------------------------------------------------------------------------------------------------------------

constexpr auto uniform = uniform_t<real_t>{};

static_assert(uniform(0.0) == 1.0);
static_assert(uniform(0.5) == 1.0);
static_assert(uniform(1.0) == 1.0);
static_assert(uniform(10.0) == 1.0);

// --------------------------------------------------------------------------------------------------------------------
// exponential_decay_t
// --------------------------------------------------------------------------------------------------------------------

struct test_vector_t
{
    real_t magnitude;
    real_t target_position;
    real_t expected;

    friend auto operator<<(std::ostream& out, test_vector_t const& src) -> std::ostream&
    {
        return out << "{.magnitude = " << src.magnitude << ", .target_position = " << src.target_position
                   << ", .expected = " << src.expected << "}";
    }
};

struct weight_functions_exponential_decay_test_t : TestWithParam<test_vector_t>
{
    real_t magnitude = GetParam().magnitude;
    real_t target_position = GetParam().target_position;
    real_t expected = GetParam().expected;

    using sut_t = exponential_decay_t<real_t>;
    sut_t sut{sut_t::from_ratio_and_halflife(12.3, 5.6)};
};

TEST_P(weight_functions_exponential_decay_test_t, result)
{
    auto const actual = sut(magnitude, target_position);

    EXPECT_DOUBLE_EQ(expected, actual);
}

// clang-format off
test_vector_t const vectors[] = {
    {0.0, 0.0,  0.000000000000000}, // ((12.3 - 1.0)*exp(-(log(2)/5.6)*0.0) + 1.0)*0.0
    {1.0, 0.0, 12.300000000000000}, // ((12.3 - 1.0)*exp(-(log(2)/5.6)*0.0) + 1.0)*1.0
    {0.0, 1.0,  0.000000000000000}, // ((12.3 - 1.0)*exp(-(log(2)/5.6)*1.0) + 1.0)*0.0
    {1.0, 1.0, 10.984425645447145}, // ((12.3 - 1.0)*exp(-(log(2)/5.6)*1.0) + 1.0)*1.0
    {7.8, 9.1, 36.375859586735004}, // ((12.3 - 1.0)*exp(-(log(2)/5.6)*9.1) + 1.0)*7.8

    // at exactly halflife, the initial amplitude should exactly halved
    {1.0, 5.6, 6.65}, // ((12.3 - 1.0)*exp(-(log(2)/5.6)*5.6) + 1.0)*1.0
};
INSTANTIATE_TEST_SUITE_P(vectors, weight_functions_exponential_decay_test_t, ValuesIn(vectors));
// clang-format on

} // namespace
} // namespace crv::weight_functions
