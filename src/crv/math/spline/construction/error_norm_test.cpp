// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_norm.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::error_norms {
namespace {

using scalar_t = float_t;

struct jet_t
{
    using value_t = scalar_t;

    value_t value;
    value_t tangent;

    friend constexpr auto primal(jet_t j) noexcept -> value_t { return j.value; }
    friend constexpr auto tangent(jet_t j) noexcept -> value_t { return j.tangent; }
};

// ====================================================================================================================
// logsumexp_floor_t
// ====================================================================================================================

namespace logsumexp_floor_tests {

struct vector_t
{
    scalar_t x;
    scalar_t y;
    scalar_t expected;
};

struct spline_error_norms_logsumexp_floor_test_t : TestWithParam<vector_t>
{
    scalar_t const x = GetParam().x;
    scalar_t const y = GetParam().y;
    scalar_t const expected = GetParam().expected;

    using sut_t = logsumexp_floor_t<scalar_t>;
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

namespace absolute_tests {

constexpr auto absolute = absolute_t{};

// typical
static_assert(abs(absolute(jet_t{5.0, 0.0}, jet_t{5.0, 0.0}) - 0.0) < 1e-9);
static_assert(abs(absolute(jet_t{5.0, 0.0}, jet_t{3.0, 0.0}) - 2.0) < 1e-9);
static_assert(abs(absolute(jet_t{3.0, 0.0}, jet_t{5.0, 0.0}) - 2.0) < 1e-9);

// negative primal
static_assert(abs(absolute(jet_t{-5.0, 0.0}, jet_t{-3.0, 0.0}) - 2.0) < 1e-9);

// tangent does not apply
static_assert(abs(absolute(jet_t{5.0, 0.0}, jet_t{5.0, 10.0}) - 0.0) < 1e-9);
static_assert(abs(absolute(jet_t{5.0, 10.0}, jet_t{5.0, 0.0}) - 0.0) < 1e-9);
static_assert(abs(absolute(jet_t{5.0, 10.0}, jet_t{5.0, 10.0}) - 0.0) < 1e-9);

} // namespace absolute_tests

// ====================================================================================================================
// first_order_absolute_t
// ====================================================================================================================

namespace first_order_aboslute_test {

// --------------------------------------------------------------------------------------------------------------------
// default weight
// --------------------------------------------------------------------------------------------------------------------

constexpr auto first_order_absolute_default_weight = first_order_absolute_t<scalar_t>{};

constexpr auto target_jet = jet_t{10.0, 1.0};
constexpr auto exact_approx = jet_t{10.0, 1.0};
constexpr auto pos_error_approx = jet_t{8.0, 1.0}; // position error = 2, tangent = 0
constexpr auto tan_error_approx = jet_t{10.0, -2.0}; // position error = 0, tangent = 3
constexpr auto mixed_error_approx_primal_dominates = jet_t{6.0, 0.0}; // position error = 4, tangent = 1
constexpr auto mixed_error_approx_tangent_dominates = jet_t{9.0, 5.0}; // position error = 1, tangent = 4

static_assert(abs(first_order_absolute_default_weight(target_jet, exact_approx) - 0.0) < 1e-9);
static_assert(abs(first_order_absolute_default_weight(target_jet, pos_error_approx) - 2.0) < 1e-9);
static_assert(abs(first_order_absolute_default_weight(target_jet, tan_error_approx) - 3.0) < 1e-9);
static_assert(
    abs(first_order_absolute_default_weight(target_jet, mixed_error_approx_primal_dominates) - 4.0) < 1e-9);
static_assert(
    abs(first_order_absolute_default_weight(target_jet, mixed_error_approx_tangent_dominates) - 4.0) < 1e-9);

// --------------------------------------------------------------------------------------------------------------------
// custom weight
// --------------------------------------------------------------------------------------------------------------------

constexpr auto first_order_absolute_custom_weight = first_order_absolute_t<scalar_t>{0.5};
constexpr auto first_order_absolute_zero_weight = first_order_absolute_t<scalar_t>{0.0};

static_assert(abs(first_order_absolute_custom_weight(target_jet, tan_error_approx) - 1.5) < 1e-9);
static_assert(
    abs(first_order_absolute_custom_weight(target_jet, mixed_error_approx_primal_dominates) - 4.0) < 1e-9);

// tangent error is completely ignored when weight is 0
static_assert(
    abs(first_order_absolute_zero_weight(target_jet, mixed_error_approx_tangent_dominates) - 1.0) < 1e-9);

} // namespace first_order_aboslute_test

// ====================================================================================================================
// first_order_relative_t
// ====================================================================================================================

namespace first_order_relative_tests {

// predictable floor for testing
struct hard_floor_t
{
    template <typename scalar_t> constexpr auto operator()(scalar_t val, scalar_t floor_val) const noexcept -> scalar_t
    {
        return max(val, floor_val);
    }
};

using sut_t = first_order_relative_t<scalar_t, hard_floor_t>;
constexpr auto sut = sut_t{.primal_floor = 2.0, .tangent_floor = 2.0, .floor = {}};

// --------------------------------------------------------------------------------------------------------------------
// standard relative scaling (targets above floor)
// --------------------------------------------------------------------------------------------------------------------

constexpr auto large_target = jet_t{10.0, 4.0};

// exact match
static_assert(abs(sut(large_target, large_target) - 0.0) < 1e-9);

// pure primal error: error is 2.0, scale is 10.0 -> rel = 0.2
static_assert(abs(sut(large_target, jet_t{8.0, 4.0}) - 0.2) < 1e-9);

// pure tangent error: error is 2.0, scale is 4.0 -> rel = 0.5
static_assert(abs(sut(large_target, jet_t{10.0, 2.0}) - 0.5) < 1e-9);

// mixed error (tangent dominates): primal rel = 0.2, tangent rel = 0.5
static_assert(abs(sut(large_target, jet_t{8.0, 2.0}) - 0.5) < 1e-9);

// mixed error (primal dominates): primal error is 6.0 (rel = 0.6), tangent error is 1.0 (rel = 0.25)
static_assert(abs(sut(large_target, jet_t{4.0, 3.0}) - 0.6) < 1e-9);

// --------------------------------------------------------------------------------------------------------------------
// floor clamping (targets below floor)
// --------------------------------------------------------------------------------------------------------------------

// both target values are 1.0, which is below the 2.0 floor.
// the floor should clamp the scale divisor to 2.0 for both.
constexpr auto small_target = jet_t{1.0, 1.0};

// primal error is 1.0. scale clamped to 2.0 -> rel = 0.5
static_assert(abs(sut(small_target, jet_t{0.0, 1.0}) - 0.5) < 1e-9);

// tangent error is 1.0. scale clamped to 2.0 -> rel = 0.5
static_assert(abs(sut(small_target, jet_t{1.0, 0.0}) - 0.5) < 1e-9);

} // namespace first_order_relative_tests

} // namespace
} // namespace crv::spline::error_norms
