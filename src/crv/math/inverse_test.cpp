// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "inverse.hpp"
#include <crv/math/abs.hpp>
#include <crv/test/test.hpp>
#include <cmath>

namespace crv {
namespace {

using real_t = float_t;
constexpr auto sut = bisect_lower_bound_t{};

auto next(real_t value) noexcept -> real_t
{
    return std::nextafter(value, value + real_t{1});
}

auto prev(real_t value) noexcept -> real_t
{
    return std::nextafter(value, value - real_t{1});
}

using monotone_t = std::function<real_t(real_t)>;

struct param_t
{
    std::optional<real_t> expected;
    real_t low;
    real_t high;
    real_t target;
    monotone_t f;

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        out << "{expected = ";
        if (src.expected) out << *src.expected;
        else out << "nullopt";
        return out << ", low = " << src.low << ", high = " << src.high << ", target = " << src.target << "}";
    }
};

struct bisect_lower_bound_test_t : TestWithParam<param_t>
{
    std::optional<real_t> expected = GetParam().expected;
    real_t low = GetParam().low;
    real_t high = GetParam().high;
    real_t target = GetParam().target;
    monotone_t f = GetParam().f;
};

TEST_P(bisect_lower_bound_test_t, expected)
{
    EXPECT_EQ(expected, sut(low, high, target, f));
}

auto const identity = [](real_t x) noexcept { return x; };
auto const x2 = [](real_t x) noexcept { return x * real_t{2.0}; };

// well-formed targets strictly inside interval
param_t const well_formed_params[] = {
    {2.0, 0.0, 4.0, 2.0, identity}, // center of interval
    {0.0, -5.0, 5.0, 0.0, identity}, // crosses zero
    {2.5, 0.0, 5.0, 5.0, x2},
};
INSTANTIATE_TEST_SUITE_P(well_formed, bisect_lower_bound_test_t, ValuesIn(well_formed_params));

// boundary conditions, exact edges of search space
param_t const boundary_conditions_params[] = {
    {0.0, 0.0, 4.0, prev(0.0), identity}, // out of bounds, left
    {0.0, 0.0, 4.0, 0.0, identity}, // left boundary
    {next(0.0), 0.0, 4.0, next(0.0), identity}, // inside left boundary
    {4.0, 0.0, 4.0, 4.0, identity}, // right boundary
    {prev(4.0), 0.0, 4.0, prev(4.0), identity}, // inside right boundary
    {std::nullopt, 0.0, 4.0, next(4.0), identity}, // out of bounds, right
};
INSTANTIATE_TEST_SUITE_P(boundary_conditions, bisect_lower_bound_test_t, ValuesIn(boundary_conditions_params));

// edge cases
param_t const edge_cases_params[] = {
    // minimum representable positive float
    {next(0.0), 0.0, 2.0, next(0.0), identity},
    {next(0.0), 0.0, 2.0, x2(next(0.0)), x2},

    // minimum intervals, 1-ulp wide
    {1.0, 1.0, next(1.0), 1.0, identity}, // left
    {next(1.0), 1.0, next(1.0), next(1.0), identity}, // right

    // exact interval
    {1.0, 1.0, 1.0, 1.0, identity},
};
INSTANTIATE_TEST_SUITE_P(edge_cases, bisect_lower_bound_test_t, ValuesIn(edge_cases_params));

// plateau that is strictly linear, goes flat at y=1.0 for x in [1.0, 3.0), then resumes linear growth
auto const plateau = [](real_t x) noexcept {
    if (x < 1.0) return x;
    if (x < 3.0) return 1.0;
    return x - 2.0;
};
param_t const plateau_params[] = {
    {0.0, 0.0, 5.0, 0.0, plateau}, // min of domain
    {next(0.0), 0.0, 5.0, next(0.0), plateau}, // first value after min

    {0.5, 0.0, 5.0, 0.5, plateau}, // on first slope
    {prev(1.0), 0.0, 5.0, prev(1.0), plateau}, // just before plateau
    {1.0, 0.0, 5.0, 1.0, plateau}, // first value on plateau
    {next(3.0), 0.0, 5.0, next(1.0), plateau}, // first value after plateau
    {3.5, 0.0, 5.0, 1.5, plateau}, // on second slope

    {prev(5.0), 0.0, 5.0, prev(prev(3.0)), plateau}, // last value before max
    {5.0, 0.0, 5.0, prev(3.0), plateau}, // 5-2 = 2.9999., midpoint rounds up; sut returns high half of interval
    {5.0, 0.0, 5.0, 3.0, plateau}, // max of domain
    {std::nullopt, 0.0, 5.0, next(3.0), plateau}, // out of bounds, right
};
INSTANTIATE_TEST_SUITE_P(plateau, bisect_lower_bound_test_t, ValuesIn(plateau_params));

//
// constexpr tests
//

namespace constexpr_tests {

auto const x_squared = [](real_t x) noexcept { return x * x; };

static_assert(0.0 == sut(0.0, 4.0, 0.0, x_squared));
static_assert(0.5 == sut(0.1, 3.9, 0.25, x_squared));
static_assert(1.0 == sut(0.2, 3.8, 1.0, x_squared));
static_assert(1.5 == sut(0.3, 3.7, 2.25, x_squared));
static_assert(2.0 == sut(0.4, 3.7, 4.0, x_squared));

static_assert(2.9 == sut(2.0, 3.0, 8.41, x_squared));
static_assert(3.0 == sut(2.1, 4.0, 9.0, x_squared));
static_assert(3.1 == sut(2.2, 5.0, 9.61, x_squared));

static_assert(!sut(2.2, 5.0, 30.0, x_squared).has_value());

} // namespace constexpr_tests

} // namespace
} // namespace crv
