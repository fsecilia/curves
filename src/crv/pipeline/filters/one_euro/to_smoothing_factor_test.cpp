// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "to_smoothing_factor.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using cutoff_step_t = fixed_t<uint64_t, 58>;
using smoothing_factor_t = fixed_t<uint64_t, 64>;

auto constexpr smoothing_factor_zero = smoothing_factor_t{};
auto constexpr smoothing_factor_half = smoothing_factor_t::literal(uint64_t{1} << 63); // 0.5
auto constexpr smoothing_factor_max = max<smoothing_factor_t>();
auto constexpr cutoff_step_max = max<cutoff_step_t>();

auto constexpr sut = to_smoothing_factor_t<smoothing_factor_t>{};

// cutoff_step = 0 -> alpha = 0
static_assert(smoothing_factor_zero == sut(cutoff_step_t{}));

// cutoff_step = 1.0 -> alpha = 0.5
// (1<<58 << 64) / (2<<58) = 2^122 / 2^59 = 2^63
static_assert(smoothing_factor_half == sut(cutoff_step_t{1}));

// max safe input still divides (no overflow), alpha strictly in (0, 1)
constexpr auto max_safe_cutoff_step = cutoff_step_max - cutoff_step_t{1};
static_assert(smoothing_factor_zero < sut(max_safe_cutoff_step));
static_assert(smoothing_factor_max > sut(max_safe_cutoff_step));

// past the safe boundary -> saturates to alpha ~= 1
static_assert(smoothing_factor_t::literal(max<uint64_t>()) == sut(max_safe_cutoff_step + cutoff_step_t::literal(1)));
static_assert(smoothing_factor_max == sut(cutoff_step_max));

// monotonic in cutoff_step
static_assert(sut(cutoff_step_t{1} >> 1) > sut(cutoff_step_t{1} >> 2));

// cutoff_step = 2.0 -> alpha = 2/3, truncated, not rounded
static_assert(0xAAAAAAAAAAAAAAAAULL == sut(cutoff_step_t{2}).value);

// cutoff_step = 31 -> alpha = 31/32, exact binary passthrough ceiling
static_assert(smoothing_factor_t::literal(uint64_t{31} << 59) == sut(cutoff_step_t{31}));

} // namespace
} // namespace crv::pipeline::filters::one_euro
