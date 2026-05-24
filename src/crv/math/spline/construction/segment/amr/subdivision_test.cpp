// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdivision.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using scalar_t = float_t;

constexpr auto sampler_id = 3;
constexpr auto mock_sampler = [] { return sampler_id; };

struct subdomain_t
{
    int_t id;
    constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
};

struct interval_t
{
    using scalar_t = scalar_t;

    subdomain_t subdomain;
    int_t factory_id;
    constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
};

using subdivision_t = subdivision_t<interval_t>;

struct bisection_t
{
    subdomain_t left;
    subdomain_t right;
};

struct bisector_t
{
    constexpr auto operator()(auto const& function_sampler, subdomain_t const& parent) const noexcept -> bisection_t
    {
        return {.left = {parent.id + 10 + function_sampler()}, .right = {parent.id + 20}};
    }
};

struct interval_factory_t
{
    using interval_t = interval_t;

    constexpr auto operator()(auto const& function_sampler, subdomain_t const& subdomain) const noexcept -> interval_t
    {
        return {.subdomain = subdomain, .factory_id = 99 + function_sampler()};
    }
};

constexpr auto parent_interval = interval_t{.subdomain = {.id = 5}, .factory_id = 0};

constexpr auto sut = subdivider_t<subdivision_t, bisector_t, interval_factory_t>{.bisect = {}, .create_interval = {}};

constexpr auto actual = sut(mock_sampler, parent_interval);

// left interval
static_assert(actual.left.subdomain.id == 18);
static_assert(actual.left.factory_id == 102);

// right interval
static_assert(actual.right.subdomain.id == 25);
static_assert(actual.right.factory_id == 102);

} // namespace
} // namespace crv::spline
