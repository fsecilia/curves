// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cutoff_step_clamp.hpp"
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using cutoff_step_value_t = uint64_t;
using cutoff_step_t = fixed_t<cutoff_step_value_t, 58>;
using alpha_t = fixed_t<uint64_t, 64>;
using scalar_t = fixed_t<uint64_t, 0>;
using magnitude_t = fixed_t<uint32_t, 16>;

constexpr auto fine_shift = 2;
using fine_step_t = fixed_t<uint128_t, cutoff_step_t::frac_bits + fine_shift>;

using sut_t = cutoff_step_clamp_t<cutoff_step_t>;

constexpr auto sut = sut_t{};
constexpr auto cutoff_ceiling = sut_t::cutoff_ceiling;
constexpr auto cutoff_ceiling_value = sut_t::cutoff_ceiling.value;

constexpr auto product_raw(cutoff_step_value_t raw) noexcept -> auto
{
    return multiply(cutoff_step_t::literal(raw), scalar_t::literal(1));
}

static_assert(sut(product_raw(cutoff_ceiling_value - 1)) == cutoff_step_t::literal(cutoff_ceiling_value - 1));
static_assert(sut(product_raw(cutoff_ceiling_value)) == cutoff_ceiling);
static_assert(sut(product_raw(cutoff_ceiling_value + 1)) == cutoff_ceiling);

// 3.0*0.5 = 1.5 cutoff-step ulps; alignment intentionally truncates it to one raw ulp
static_assert(sut(multiply(cutoff_step_t::literal(3), magnitude_t{1} >> 1)) == cutoff_step_t::literal(2));

// ceiling - 0.25 ulp is in range and rounds to the ceiling.
constexpr auto ceiling_minus_quarter_ulp
    = fine_step_t::literal((static_cast<uint128_t>(cutoff_ceiling_value) << fine_shift) - 1);
static_assert(sut(ceiling_minus_quarter_ulp) == cutoff_ceiling);

// ceiling + 0.25 ulp is out of range and is clamped
constexpr auto ceiling_plus_quarter_ulp
    = fine_step_t::literal((static_cast<uint128_t>(cutoff_ceiling_value) << fine_shift) + 1);
static_assert(sut(ceiling_plus_quarter_ulp) == cutoff_ceiling);

} // namespace
} // namespace crv::pipeline::filters::one_euro
