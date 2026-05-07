// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "function_sampler.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;
using function_sample_t = function_sample_t<real_t>;

struct target_function_t
{
    constexpr auto operator()(jet_t x) const noexcept -> jet_t { return {x.f * 2, x.df * 16}; }
};

using sut_t = function_sampler_t<target_function_t>;
constexpr auto sut = sut_t{target_function_t{}};

static_assert(function_sample_t{.x = 0.0, .y = jet_t{0.0, 16.0}} == sut(0.0));
static_assert(function_sample_t{.x = 2.0, .y = jet_t{4.0, 16.0}} == sut(2.0));

} // namespace
} // namespace crv::spline
