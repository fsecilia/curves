// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdivision_predicate.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;

struct subdomain_t
{
    int log2_width;
};

struct residual_t
{
    scalar_t scale;
    scalar_t metric_error;
};

struct interval_t
{
    subdomain_t subdomain;
    residual_t residual;
};

constexpr auto global_tolerance = 1e-4;
constexpr auto log2_min_width = 2;
constexpr auto sut = subdivision_predicate_t<scalar_t, log2_min_width>{.global_tolerance = global_tolerance};

// nominal subdivision; interval is wide enough and error exceeds both global tolerance and noise floor
static_assert(sut(interval_t{.subdomain = {.log2_width = 4}, .residual = {.scale = 1.0, .metric_error = 1.0}}));

// width limit exhaustion; error is massive, but the interval cannot be subdivided any further
static_assert(
    !sut(interval_t{.subdomain = {.log2_width = log2_min_width}, .residual = {.scale = 1.0, .metric_error = 1.0}}));

// global tolerance satisfaction; interval is wide, but the error is within the acceptable global margin
static_assert(!sut(interval_t{.subdomain = {.log2_width = 4}, .residual = {.scale = 1.0, .metric_error = 1e-5}}));

// submerged in noise floor
// double epsilon is around 2.22e-16, unscaled noise floor is ~2.22e-16 * 64 ~= 1.42e-14
// scaling that by 1e-6 gives 1.42e-8
// error of 5e-9 exceeds this
static_assert(!sut(interval_t{.subdomain = {.log2_width = 4}, .residual = {.scale = 1e6, .metric_error = 5e-9}}));

} // namespace
} // namespace crv::spline
