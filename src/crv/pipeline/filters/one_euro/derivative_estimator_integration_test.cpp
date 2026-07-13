// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "derivative_estimator.hpp"
#include <crv/pipeline/filters/one_euro/ema_accumulator.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using x_t = fixed_t<int32_t, 20>;
using dx_t = fixed_t<int32_t, 16>;
using reciprocal_dt_ms_t = fixed_t<uint32_t, 18>;
using alpha_t = fixed_t<uint32_t, 32>;

using sut_t = derivative_estimator_t<x_t, dx_t, ema_accumulator_t<dx_t>>;

constexpr auto reciprocal_quarter = reciprocal_dt_ms_t{1} / 4;
constexpr auto alpha_half = alpha_t::literal(uint32_t{1} << 31);
constexpr auto dx_half = dx_t{1} >> 1;

constexpr auto composes_with_real_ema_accumulator() noexcept -> bool
{
    auto sut = sut_t{};

    // derivative: (8 - 0) * 0.25 = 2
    // ema: 0 + 0.5 * (2 - 0) = 1
    if (sut(x_t{8}, reciprocal_quarter, alpha_half) != dx_t{1}) return false;
    if (sut.prev() != x_t{8}) return false;
    if (sut.output() != dx_t{1}) return false;

    // derivative: (8 - 8) * 0.25 = 0
    // ema: 1 + 0.5 * (0 - 1) = 0.5
    if (sut(x_t{8}, reciprocal_quarter, alpha_half) != dx_half) { return false; }

    return sut.prev() == x_t{8} && sut.output() == dx_half;
}
static_assert(composes_with_real_ema_accumulator());

} // namespace
} // namespace crv::pipeline::filters::one_euro
