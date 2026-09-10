// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extension.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/spline/segment.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 25>;
using significand_t = int64_t;
using unpacked_field_t = unpacked_field_t<significand_t>;

using sut_t = extended_tangent_t<x_t, y_t, unpacked_field_t>;

constexpr auto x_inf = max<x_t>();

// encoded tangent validation
constexpr auto validate = sut_t::validator_t{};
static_assert(validate(sut_t{.slope = {.significand = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}));
static_assert(!validate(sut_t{.slope = {.significand = -1, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}));
static_assert(!validate(sut_t{.slope = {.significand = 1, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_t::literal(-1)}));
static_assert(!validate(sut_t{.slope = {.significand = 1, .shift = 128}, .y0 = y_t{3}, .x_max_delta = x_t{1}}));
static_assert(
    !validate(sut_t{.slope = {.significand = 1, .shift = min<int_t>()}, .y0 = y_t{3}, .x_max_delta = x_t{1}}));

// right-shift narrowing boundary: max*max needs 63 bits of right shift to fit int64
using integer_x_t = fixed_t<int64_t, 0>;
using integer_y_t = fixed_t<int64_t, 0>;
using integer_sut_t = extended_tangent_t<integer_x_t, integer_y_t, unpacked_field_t>;
using truncating_integer_sut_t
    = extended_tangent_t<integer_x_t, integer_y_t, unpacked_field_t, rounding_modes::shr::truncate>;
constexpr auto validate_integer = integer_sut_t::validator_t{};
constexpr auto integer_max = max<int64_t>();
static_assert(validate_integer(integer_sut_t{
    .slope = {.significand = integer_max, .shift = 63},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal(integer_max),
}));
static_assert(!validate_integer(integer_sut_t{
    .slope = {.significand = integer_max, .shift = 62},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal(integer_max),
}));

// wider x domain exposes product and rounding-headroom validation independently
using wide_x_t = fixed_t<int128_t, 0>;
using wide_x_sut_t = extended_tangent_t<wide_x_t, integer_y_t, unpacked_field_t>;
constexpr auto validate_wide_x = wide_x_sut_t::validator_t{};
static_assert(!validate_wide_x(wide_x_sut_t{
    .slope = {.significand = 2, .shift = 0},
    .y0 = integer_y_t{},
    .x_max_delta = max<wide_x_t>(),
}));
static_assert(!validate_wide_x(wide_x_sut_t{
    .slope = {.significand = 1, .shift = 1},
    .y0 = integer_y_t{},
    .x_max_delta = max<wide_x_t>(),
}));

// left-shift arithmetic width and represented-output boundaries
static_assert(!validate_integer(integer_sut_t{
    .slope = {.significand = 1, .shift = -128},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t{1},
}));
static_assert(validate_integer(integer_sut_t{
    .slope = {.significand = 1, .shift = -1},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal(integer_max >> 1),
}));
static_assert(!validate_integer(integer_sut_t{
    .slope = {.significand = 1, .shift = -1},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t::literal((integer_max >> 1) + 1),
}));

// representative finite clamp remains valid
static_assert(validate_integer(integer_sut_t{
    .slope = {.significand = 3, .shift = 1},
    .y0 = integer_y_t{7},
    .x_max_delta = integer_x_t{11},
}));

struct tangent_validator_rejection_test_t : testing::TestWithParam<integer_sut_t>
{};

TEST_P(tangent_validator_rejection_test_t, rejects_malformed_encoding)
{
    EXPECT_FALSE(integer_sut_t::validator_t{}(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(malformed_encodings, tangent_validator_rejection_test_t,
    testing::Values(
        integer_sut_t{.slope = {.significand = -1, .shift = 0}, .y0 = integer_y_t{}, .x_max_delta = integer_x_t{1}},
        integer_sut_t{
            .slope = {.significand = 1, .shift = 0},
            .y0 = integer_y_t{},
            .x_max_delta = integer_x_t::literal(-1),
        },
        integer_sut_t{.slope = {.significand = 1, .shift = 128}, .y0 = integer_y_t{}, .x_max_delta = integer_x_t{1}},
        integer_sut_t{
            .slope = {.significand = 1, .shift = min<int_t>()},
            .y0 = integer_y_t{},
            .x_max_delta = integer_x_t{1},
        },
        integer_sut_t{
            .slope = {.significand = integer_max, .shift = 62},
            .y0 = integer_y_t{},
            .x_max_delta = integer_x_t::literal(integer_max),
        },
        integer_sut_t{.slope = {.significand = 1, .shift = -128}, .y0 = integer_y_t{}, .x_max_delta = integer_x_t{1}},
        integer_sut_t{
            .slope = {.significand = 1, .shift = -1},
            .y0 = integer_y_t{},
            .x_max_delta = integer_x_t::literal((integer_max >> 1) + 1),
        }));

struct wide_x_tangent_validator_rejection_test_t : testing::TestWithParam<wide_x_sut_t>
{};

TEST_P(wide_x_tangent_validator_rejection_test_t, rejects_malformed_encoding)
{
    EXPECT_FALSE(wide_x_sut_t::validator_t{}(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(wide_arithmetic_failures, wide_x_tangent_validator_rejection_test_t,
    testing::Values(
        wide_x_sut_t{.slope = {.significand = 2, .shift = 0}, .y0 = integer_y_t{}, .x_max_delta = max<wide_x_t>()},
        wide_x_sut_t{.slope = {.significand = 1, .shift = 1}, .y0 = integer_y_t{}, .x_max_delta = max<wide_x_t>()}));

struct tangent_validator_acceptance_test_t : testing::TestWithParam<integer_sut_t>
{};

TEST_P(tangent_validator_acceptance_test_t, accepts_safe_encoding)
{
    EXPECT_TRUE(integer_sut_t::validator_t{}(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(safe_encodings, tangent_validator_acceptance_test_t,
    testing::Values(
        integer_sut_t{
            .slope = {.significand = 0, .shift = 0},
            .y0 = integer_y_t{3},
            .x_max_delta = max<integer_x_t>(),
        },
        integer_sut_t{
            .slope = {.significand = integer_max, .shift = 63},
            .y0 = integer_y_t{},
            .x_max_delta = integer_x_t::literal(integer_max),
        },
        integer_sut_t{
            .slope = {.significand = 1, .shift = -1},
            .y0 = integer_y_t{},
            .x_max_delta = integer_x_t::literal(integer_max >> 1),
        },
        integer_sut_t{.slope = {.significand = 3, .shift = 1}, .y0 = integer_y_t{7}, .x_max_delta = integer_x_t{11}}));

// arithmetic-width boundary is later than the observable-output boundary
static_assert(integer_sut_t{
                  .slope = {.significand = integer_max, .shift = 126},
                  .y0 = integer_y_t{},
                  .x_max_delta = integer_x_t::literal(integer_max),
              }(integer_x_t::literal(integer_max))
    == integer_y_t{1});
static_assert(integer_sut_t{
                  .slope = {.significand = integer_max, .shift = 127},
                  .y0 = integer_y_t{},
                  .x_max_delta = integer_x_t::literal(integer_max),
              }(integer_x_t::literal(integer_max))
    == integer_y_t{0});

// rounding mode controls runtime right shifts
static_assert(truncating_integer_sut_t{
                  .slope = {.significand = 3, .shift = 1},
                  .y0 = integer_y_t{},
                  .x_max_delta = integer_x_t{1},
              }(integer_x_t{1})
    == integer_y_t{1});

// nearest-up first output increment occurs exactly at the half-way product
constexpr auto nearest_up_boundary_tangent = integer_sut_t{
    .slope = {.significand = 1, .shift = 4},
    .y0 = integer_y_t{},
    .x_max_delta = integer_x_t{9},
};
static_assert(nearest_up_boundary_tangent(integer_x_t{7}) == integer_y_t{0});
static_assert(nearest_up_boundary_tangent(integer_x_t{8}) == integer_y_t{1});
static_assert(nearest_up_boundary_tangent(integer_x_t{9}) == integer_y_t{1});

// constant slope
// y = 0.0*x + 3.0
static_assert(sut_t{.slope = {.significand = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}(x_t{5}) == y_t{3});
static_assert(sut_t{.slope = {.significand = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}(x_t{7}) == y_t{3});

// positive slope
// y = 1.0*x + 0.0
// required shift = 45 - 26 = 19
static_assert(
    sut_t{.slope = {.significand = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{5}) == y_t{5});
static_assert(
    sut_t{.slope = {.significand = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{7}) == y_t{7});

// y = 1.0*x + 0.0, represented with an exact left shift at runtime.
static_assert(
    sut_t{.slope = {.significand = 1LL << 10, .shift = -1}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{5}) == y_t{5});

// y = 1.0*x + 0.0, shift = 19. Clamp at x=5.
static_assert(
    sut_t{.slope = {.significand = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_t{5}}(x_t{3}) == y_t{3});
static_assert(
    sut_t{.slope = {.significand = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_t{5}}(x_t{10}) == y_t{5});

// clamp distance is derived from the stored quantized line
static_assert(sut_t::clamp_delta({.significand = 1LL << 30, .shift = 19}, y_t{3}, y_t{5}) == x_t{2});
static_assert(sut_t::clamp_delta({.significand = 1LL << 10, .shift = -1}, y_t{3}, y_t{5}) == x_t{2});
static_assert(sut_t::clamp_delta({.significand = 0, .shift = 0}, y_t{3}, y_t{5}) == x_inf);
static_assert(sut_t::clamp_delta({.significand = 1, .shift = 0}, y_t{5}, y_t{5}) == x_t{0});

// bounded evaluator reaches, but never numerically crosses, upper output bound
static_assert(
    sut_t{.slope = {.significand = 1LL << 30, .shift = 19}, .y0 = y_t{3}, .x_max_delta = x_t{2}}(x_t{3}) == y_t{5});

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(tangent_extension_test_t, rejects_negative_slope)
{
    EXPECT_DEATH(
        static_cast<void>(sut_t::clamp_delta({.significand = -1, .shift = 0}, y_t{3}, y_t{5})), "slope.significand");
}

#endif // defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
