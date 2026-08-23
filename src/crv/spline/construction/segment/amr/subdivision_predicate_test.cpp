// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdivision_predicate.hpp"
#include <crv/test/test.hpp>
#include <optional>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using x_t = fixed_t<int_t, 0>;

struct subdomain_t
{
    x_t left_x;
    x_t midpoint_x;
    x_t right_x;
};

struct residual_t
{
    scalar_t scale;
    scalar_t metric_error;
};

struct interval_t
{
    subdomain_t subdomain;
    std::optional<residual_t> residual;
};

constexpr auto global_tolerance = 1e-4;
constexpr auto log2_min_width = 1; // min AMR-created child width = 2
constexpr auto sut = subdivision_predicate_t<scalar_t, x_t, log2_min_width>{.global_tolerance = global_tolerance};

static_assert(sut(interval_t{.subdomain = {x_t{0}, x_t{4}, x_t{9}}, .residual = residual_t{.scale = 1.0, .metric_error = 1.0}}));

// odd width is fine when both resulting children satisfy min_width
static_assert(sut(interval_t{.subdomain = {x_t{0}, x_t{2}, x_t{5}}, .residual = residual_t{.scale = 1.0, .metric_error = 1.0}}));

// min_width constrains newly created children, not the location of existing knots
static_assert(!sut(interval_t{.subdomain = {x_t{0}, x_t{1}, x_t{3}}, .residual = residual_t{.scale = 1.0, .metric_error = 1.0}}));

// no distinct representable midpoint
static_assert(!sut(interval_t{.subdomain = {x_t{0}, x_t{0}, x_t{1}}, .residual = residual_t{.scale = 1.0, .metric_error = 1.0}}));

static_assert(
    !sut(interval_t{.subdomain = {x_t{0}, x_t{4}, x_t{9}}, .residual = residual_t{.scale = 1.0, .metric_error = 1e-5}}));
static_assert(
    !sut(interval_t{.subdomain = {x_t{0}, x_t{4}, x_t{9}}, .residual = residual_t{.scale = 1e6, .metric_error = 5e-9}}));

} // namespace
} // namespace crv::spline
