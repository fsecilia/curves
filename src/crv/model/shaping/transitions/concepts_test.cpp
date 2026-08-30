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

struct antiderivative_t
{
    auto domain_end() const noexcept -> float_t;
    auto operator()(float_t) const noexcept -> float_t;
};

static_assert(is_transition<nast_t<float_t, antiderivative_t>, float_t>);
static_assert(is_transition<smoothstep_t, float_t>);
static_assert(is_transition<smootherstep_t, float_t>);
static_assert(is_transition<smootheststep_t, float_t>);

struct missing_antiderivative_t
{
    auto value(float_t) const noexcept -> float_t;
    auto value(jet_t<float_t>) const noexcept -> jet_t<float_t>;
    auto derivative(float_t) const noexcept -> float_t;
};

static_assert(!is_transition<missing_antiderivative_t, float_t>);

struct throwing_transition_t
{
    auto value(float_t) const -> float_t;
    auto value(jet_t<float_t>) const noexcept -> jet_t<float_t>;
    auto derivative(float_t) const noexcept -> float_t;
    auto antiderivative(float_t) const noexcept -> float_t;
    auto antiderivative(jet_t<float_t>) const noexcept -> jet_t<float_t>;
};

static_assert(!is_transition<throwing_transition_t, float_t>);

} // namespace
} // namespace crv::shaping::transitions
