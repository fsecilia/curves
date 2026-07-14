// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cutoff_step_calculator.hpp"
#include <crv/pipeline/filters/one_euro/cutoff_rate_combiner.hpp>
#include <crv/pipeline/filters/one_euro/cutoff_step_clamp.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::pipeline::filters::one_euro {
namespace {

using dx_t = fixed_t<int32_t, 16>;
using dt_t = fixed_t<uint64_t, 0>;
using cutoff_step_t = fixed_t<uint64_t, 58>;

using sut_t = cutoff_step_calculator_t<cutoff_step_t, cutoff_rate_combiner_t<cutoff_step_t>,
    cutoff_step_clamp_t<cutoff_step_t>>;
constexpr auto sut = sut_t{};
constexpr auto omega_one = cutoff_step_t{1};
constexpr auto dt_five = dt_t::literal(5);

// nominal composition, including the negative-derivative magnitude path
static_assert(sut(omega_one, omega_one, dx_t{-1}, dt_five) == cutoff_step_t{10});

// end-to-end saturation through real clamp
static_assert(sut(omega_one, omega_one, dx_t{1000}, dt_five) == cutoff_step_t{31});

} // namespace
} // namespace crv::pipeline::filters::one_euro
