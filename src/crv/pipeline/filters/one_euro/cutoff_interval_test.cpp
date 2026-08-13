// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cutoff_interval.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using dt_ns_t = fixed_t<uint8_t, 0>;

using cutoff_rate_t = fixed_t<int8_t, 4>;
using cutoff_interval_value_t = fixed_t<int8_t, 4>;
using cutoff_interval_t = one_euro::cutoff_interval_t<cutoff_interval_value_t>;
static_assert(cutoff_interval_t::maximum_finite == cutoff_interval_value_t::literal(111));

// 3/16 * 5 = 15/16.
constexpr auto ordinary = make_cutoff_interval<cutoff_interval_value_t>(cutoff_rate_t::literal(3), dt_ns_t{5});
static_assert(!ordinary.is_limit);
static_assert(ordinary.value == cutoff_interval_value_t::literal(15));

constexpr auto smallest = make_cutoff_interval<cutoff_interval_value_t>(cutoff_rate_t::literal(1), dt_ns_t{1});
static_assert(!smallest.is_limit);
static_assert(smallest.value == cutoff_interval_value_t::literal(1));

constexpr auto maximum_finite = make_cutoff_interval<cutoff_interval_value_t>(
    cutoff_rate_t::literal(cutoff_interval_t::maximum_finite.value), dt_ns_t{1});
static_assert(!maximum_finite.is_limit);
static_assert(maximum_finite.value == cutoff_interval_t::maximum_finite);
static_assert(maximum_finite.value + cutoff_interval_value_t{1} == max<cutoff_interval_value_t>());

constexpr auto first_above_maximum_finite = make_cutoff_interval<cutoff_interval_value_t>(
    cutoff_rate_t::literal(cutoff_interval_t::maximum_finite.value + 1), dt_ns_t{1});
static_assert(first_above_maximum_finite.is_limit);

// The interval has two more fractional bits than the cutoff-rate product:
//
//     maximum_finite       = 6.9375
//     truncated ceiling    = 6.75
//     next product value   = 7.0
//
// A nearest conversion of maximum_finite to the product representation would
// produce 7.0 and incorrectly admit a value whose denominator +1 is not
// representable.
using coarse_cutoff_rate_t = fixed_t<int8_t, 2>;
using fine_cutoff_interval_value_t = fixed_t<int8_t, 4>;

constexpr auto truncated_ceiling
    = make_cutoff_interval<fine_cutoff_interval_value_t>(coarse_cutoff_rate_t::literal(27), dt_ns_t{1});
static_assert(!truncated_ceiling.is_limit);
static_assert(truncated_ceiling.value == fine_cutoff_interval_value_t::literal(108));

constexpr auto first_above_truncated_ceiling
    = make_cutoff_interval<fine_cutoff_interval_value_t>(coarse_cutoff_rate_t::literal(28), dt_ns_t{1});
static_assert(first_above_truncated_ceiling.is_limit);

} // namespace
} // namespace crv::pipeline::filters::one_euro
