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

static_assert(is_transition<nast_t<float32_t>, float32_t>);
static_assert(is_transition<nast_t<float64_t>, float64_t>);
static_assert(is_transition<smoothstep_t, float32_t>);
static_assert(is_transition<smoothstep_t, float64_t>);
static_assert(is_transition<smootherstep_t, float32_t>);
static_assert(is_transition<smootherstep_t, float64_t>);
static_assert(is_transition<smootheststep_t, float32_t>);
static_assert(is_transition<smootheststep_t, float64_t>);

struct missing_antiderivative_t
{
    auto operator()(float_t) const noexcept -> float_t;
    auto operator()(jet_t<float_t>) const noexcept -> jet_t<float_t>;
    auto derivative(float_t) const noexcept -> float_t;
};

static_assert(!is_transition<missing_antiderivative_t, float_t>);

} // namespace
} // namespace crv::shaping::transitions
