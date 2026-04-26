// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "weight_function.hpp"
#include <crv/test/test.hpp>

namespace crv::weight_functions {
namespace {

using real_t = float_t;

struct test_vector_t
{
    real_t node;
    real_t expected;

    friend auto operator<<(std::ostream& out, test_vector_t const& src) -> std::ostream&
    {
        return out << "{.node = " << src.node << ", .expected = " << src.expected << "}";
    }
};

// ====================================================================================================================
// uniform_t
// ====================================================================================================================

constexpr auto uniform = uniform_t<real_t>{};

static_assert(uniform(0.0) == 1.0);
static_assert(uniform(0.5) == 1.0);
static_assert(uniform(1.0) == 1.0);
static_assert(uniform(10.0) == 1.0);

// ====================================================================================================================
// exponential_decay_t
// ====================================================================================================================

struct weight_functions_exponential_decay_test_t : Test
{
    using sut_t = exponential_decay_t<real_t>;
};

TEST_F(weight_functions_exponential_decay_test_t, from_ratio_and_halflife)
{
    auto const actual = sut_t::from_ratio_and_halflife(12.3, 5.6);
    EXPECT_DOUBLE_EQ(11.3, actual.amplitude);
    EXPECT_DOUBLE_EQ(std::log(2.0) / 5.6, actual.decay_rate);
    EXPECT_DOUBLE_EQ(1.0, actual.baseline_offset);
}

// --------------------------------------------------------------------------------------------------------------------
// vector_test
// --------------------------------------------------------------------------------------------------------------------

struct weight_functions_exponential_decay_vector_test_t : weight_functions_exponential_decay_test_t,
                                                          WithParamInterface<test_vector_t>
{
    real_t node = GetParam().node;
    real_t expected = GetParam().expected;

    using sut_t = exponential_decay_t<real_t>;
    sut_t sut{.amplitude = 7.11, .decay_rate = std::log(2.0) / 13.17, .baseline_offset = 2.3};
};

TEST_P(weight_functions_exponential_decay_vector_test_t, result)
{
    auto const actual = sut(node);

    EXPECT_DOUBLE_EQ(expected, actual);
}

// clang-format off
test_vector_t const vectors[] = {
    { 0.00, 9.410000000000000}, // 7.11*exp(-(log(2)/13.17)* 0.00) + 2.3
    { 1.00, 9.045472090551783}, // 7.11*exp(-(log(2)/13.17)* 1.00) + 2.3
    { 3.10, 8.339654878652086}, // 7.11*exp(-(log(2)/13.17)* 3.10) + 2.3
    {41.43, 3.103329456213085}, // 7.11*exp(-(log(2)/13.17)*41.43) + 2.3

    // at exactly halflife, the initial amplitude should exactly halved
    {13.17, 5.855}, // 7.11*exp(-(log(2)/13.17)*13.17) + 2.3
};
INSTANTIATE_TEST_SUITE_P(vectors, weight_functions_exponential_decay_vector_test_t, ValuesIn(vectors));
// clang-format on

// ====================================================================================================================
// hyperbolic_decay_t
// ====================================================================================================================

static_assert(hyperbolic_decay_t<real_t>{0}(1.0) == 1.0 / 1.0);
static_assert(hyperbolic_decay_t<real_t>{0}(2.0) == 1.0 / 2.0);
static_assert(hyperbolic_decay_t<real_t>{0}(7.0) == 1.0 / 7.0);
static_assert(hyperbolic_decay_t<real_t>{0}(11.0) == 1.0 / 11.0);

static_assert(hyperbolic_decay_t<real_t>{3.1}(1.0) == 1.0 / (1.0 + 3.1));
static_assert(hyperbolic_decay_t<real_t>{3.1}(2.0) == 1.0 / (2.0 + 3.1));
static_assert(hyperbolic_decay_t<real_t>{3.1}(7.0) == 1.0 / (7.0 + 3.1));
static_assert(hyperbolic_decay_t<real_t>{3.1}(11.0) == 1.0 / (11.0 + 3.1));

} // namespace
} // namespace crv::weight_functions
