// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "function_sampler.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;

struct target_function_t
{
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t { return x * 3; }
    constexpr auto operator()(jet_t x) const noexcept -> jet_t { return {x.f * 2, x.df * 16}; }
};

using sut_t = function_sampler_t<target_function_t>;
constexpr auto sut = sut_t{target_function_t{}};

static_assert(function_sample_t<scalar_t>{.x = 0.0, .y = 0.0} == sut(0.0));
static_assert(function_sample_t<scalar_t>{.x = 5.0, .y = 15.0} == sut(5.0));
static_assert(function_sample_t<jet_t>{.x = 0.0, .y = jet_t{0.0, 48.0}} == sut(jet_t{0.0, 3.0}));
static_assert(function_sample_t<jet_t>{.x = 5.0, .y = jet_t{10.0, 112.0}} == sut(jet_t{5.0, 7.0}));

} // namespace
} // namespace crv::spline
