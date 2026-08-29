// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "concepts.hpp"
#include <crv/model/shaping/transitions/nast.hpp>
#include <crv/model/shaping/transitions/smootherstep.hpp>
#include <crv/model/shaping/transitions/smootheststep.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/test/test.hpp>

namespace crv::shaping::transitions {
namespace {

template <std::floating_point t_scalar_t> struct antiderivative_t
{
    using scalar_t = t_scalar_t;

    auto domain_end() const noexcept -> scalar_t;
    auto operator()(scalar_t) const noexcept -> scalar_t;
};

static_assert(is_transition<nast_t<float32_t, antiderivative_t<float32_t>>, float32_t>);
static_assert(is_transition<nast_t<float64_t, antiderivative_t<float64_t>>, float64_t>);
static_assert(is_transition<smoothstep_t, float32_t>);
static_assert(is_transition<smoothstep_t, float64_t>);
static_assert(is_transition<smootherstep_t, float32_t>);
static_assert(is_transition<smootherstep_t, float64_t>);
static_assert(is_transition<smootheststep_t, float32_t>);
static_assert(is_transition<smootheststep_t, float64_t>);

struct missing_antiderivative_t
{
    auto value(float_t) const noexcept -> float_t;
    auto value(jet_t<float_t>) const noexcept -> jet_t<float_t>;
    auto derivative(float_t) const noexcept -> float_t;
};

static_assert(!is_transition<missing_antiderivative_t, float_t>);

} // namespace
} // namespace crv::shaping::transitions
