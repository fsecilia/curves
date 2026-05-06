// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_norm.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::error_norms {
namespace {

using real_t = float_t;

struct jet_t
{
    using value_t = real_t;

    value_t value;
    value_t tangent;

    friend constexpr auto primal(jet_t j) noexcept -> value_t { return j.value; }
    friend constexpr auto tangent(jet_t j) noexcept -> value_t { return j.tangent; }
};

// ====================================================================================================================
// logsumexp_floor
// ====================================================================================================================

namespace logsumexp_floor_tests {

struct vector_t
{
    real_t x;
    real_t y;
    real_t expected;
};

struct spline_error_norms_logsumexp_floor_test_t : TestWithParam<vector_t>
{
    real_t const x = GetParam().x;
    real_t const y = GetParam().y;
    real_t const expected = GetParam().expected;

    using sut_t = logsumexp_floor_t<real_t>;
    sut_t sut{};
};

TEST_P(spline_error_norms_logsumexp_floor_test_t, vector)
{
    auto const actual = sut(x, y);
    EXPECT_NEAR(expected, actual, 1e-10);
}

// These values were generated using wolfram alpha:
// log(exp(10*x) + exp(10*y)) / 10 where (x, y) = (1.0, 1.0)
vector_t const vectors[] = {
    // maximum deviation
    // lse(x, x) = x + ln(2)/k
    {1.0, 1.0, 1.0693147180559945},
    {0.001, 0.001, 0.07031471805599453},

    // y dominates
    // output should asymptotically approch y
    {0.1, 2.0, 2.00000000056028},
    {0.00001, 0.01, 0.07444441634017054},

    // x dominates
    // output should asymptotically approach x
    {2.0, 0.1, 2.00000000056028},
    {1.5, 0.001, 1.5000000308976642},

    // transition zone
    // tests curvature near threshold
    {1.2, 1.0, 1.2126928011042972},
    {0.8, 1.0, 1.0126928011042973},

    // overflow checks
    // these values overflow without the stability trick
    {100.0, 1.0, 100.0000000000000000},
    {1.0, 100.0, 100.0000000000000000},
};
INSTANTIATE_TEST_SUITE_P(vectors, spline_error_norms_logsumexp_floor_test_t, ValuesIn(vectors));

} // namespace logsumexp_floor_tests

// ====================================================================================================================
// absolute_t
// ====================================================================================================================

constexpr auto absolute = absolute_t{};

// typical
static_assert(abs(absolute(jet_t{5.0, 0.0}, jet_t{5.0, 0.0}) - 0.0) < 1e-9);
static_assert(abs(absolute(jet_t{5.0, 0.0}, jet_t{3.0, 0.0}) - 2.0) < 1e-9);
static_assert(abs(absolute(jet_t{3.0, 0.0}, jet_t{5.0, 0.0}) - 2.0) < 1e-9);

// tangent does not apply
static_assert(abs(absolute(jet_t{5.0, 0.0}, jet_t{5.0, 10.0}) - 0.0) < 1e-9);
static_assert(abs(absolute(jet_t{5.0, 10.0}, jet_t{5.0, 0.0}) - 0.0) < 1e-9);
static_assert(abs(absolute(jet_t{5.0, 10.0}, jet_t{5.0, 10.0}) - 0.0) < 1e-9);

// ====================================================================================================================
// first_order_absolute_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// default weight
// --------------------------------------------------------------------------------------------------------------------

constexpr auto first_order_absolute_default_weight = first_order_absolute_t<real_t>{};

constexpr auto target_jet = jet_t{10.0, 1.0};
constexpr auto exact_approx = jet_t{10.0, 1.0};
constexpr auto pos_error_approx = jet_t{8.0, 1.0}; // position error = 2, tangent = 0
constexpr auto tan_error_approx = jet_t{10.0, -2.0}; // position error = 0, tangent = 3
constexpr auto mixed_error_approx = jet_t{6.0, 0.0}; // position error = 4, tangent = 1

static_assert(abs(first_order_absolute_default_weight(target_jet, exact_approx) - 0.0) < 1e-9);
static_assert(abs(first_order_absolute_default_weight(target_jet, pos_error_approx) - 2.0) < 1e-9);
static_assert(abs(first_order_absolute_default_weight(target_jet, tan_error_approx) - 3.0) < 1e-9);
static_assert(abs(first_order_absolute_default_weight(target_jet, mixed_error_approx) - 4.0) < 1e-9);

// --------------------------------------------------------------------------------------------------------------------
// custom weight
// --------------------------------------------------------------------------------------------------------------------

constexpr auto first_order_absolute_custom_weight = first_order_absolute_t<real_t>{0.5};

static_assert(abs(first_order_absolute_custom_weight(target_jet, tan_error_approx) - 1.5) < 1e-9);
static_assert(abs(first_order_absolute_custom_weight(target_jet, mixed_error_approx) - 4.0) < 1e-9);

} // namespace
} // namespace crv::spline::error_norms
