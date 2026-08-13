// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <cassert>

namespace crv::pipeline::filters::one_euro {

/// Finite dimensionless cutoff interval or its unrepresentably large limiting case.
///
/// A finite value always leaves room for the denominator's `+1`.
template <is_fixed t_value_t>
    requires(is_signed_v<t_value_t>)
struct cutoff_interval_t
{
    using value_t = t_value_t;

    /// Largest finite interval for which `value + 1` is representable.
    static constexpr auto maximum_finite = max<value_t>() - value_t{1};

    value_t value{};
    bool is_limit{};
};

/// Forms the dimensionless cutoff interval `cutoff_rate*dt_ns`.
///
/// Values too large for the finite working representation are represented explicitly as the mathematical
/// cutoff-interval limit. Finite values always satisfy:
///
///     0 < value <= maximum_finite
///
/// so `value + 1` is representable.
///
/// \pre cutoff_rate > 0
/// \pre dt_ns > 0
template <is_fixed t_cutoff_interval_value_t, is_fixed cutoff_rate_t, is_fixed dt_ns_t>
    requires(is_signed_v<t_cutoff_interval_value_t> && is_signed_v<cutoff_rate_t> && !is_signed_v<dt_ns_t>)
constexpr auto make_cutoff_interval(cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) noexcept
    -> cutoff_interval_t<t_cutoff_interval_value_t>
{
    using cutoff_interval_value_t = t_cutoff_interval_value_t;
    using output_t = cutoff_interval_t<cutoff_interval_value_t>;
    using product_t = fixed::product_t<cutoff_rate_t, dt_ns_t>;

    static_assert(cutoff_interval_value_t::int_bits > 0, "cutoff interval must represent one");
    static_assert(cutoff_interval_value_t::frac_bits >= cutoff_rate_t::frac_bits,
        "cutoff interval must preserve cutoff-rate precision for positive integer dt");
    static_assert(product_t::int_bits >= cutoff_interval_value_t::int_bits,
        "cutoff-rate/time product must contain the finite cutoff-interval range");
    static_assert(dt_ns_t::frac_bits == 0, "elapsed nanoseconds must use an integer representation");

    assert(cutoff_rate > cutoff_rate_t{});
    assert(dt_ns > dt_ns_t{});

    auto const product = multiply(cutoff_rate, dt_ns);

    // The ceiling comparison must be conservative. maximum_finite can have more fractional precision than product_t,
    // so explicitly truncate it into the product representation rather than allowing a nearest conversion to round the
    // ceiling upward.
    static constexpr auto product_ceiling
        = product_t::template convert<shifter_t<rounding_modes::shr::truncate>{}>(output_t::maximum_finite);

    // Converting the truncated ceiling back into the working interval cannot cross maximum_finite. This is the property
    // that guarantees the later denominator addition is safe.
    static_assert(cutoff_interval_value_t::convert(product_ceiling) <= output_t::maximum_finite);

    if (product > product_ceiling) return {.is_limit = true};

    // dt_ns is a positive integer and the working interval has at least the cutoff rate's fractional precision, so a
    // positive representable cutoff rate cannot underflow to zero here.
    auto const value
        = cutoff_interval_value_t::template convert<shifter_t<rounding_modes::shr::nearest_even>{}>(product);

    assert(value > cutoff_interval_value_t{});
    assert(value <= output_t::maximum_finite);

    return {.value = value};
}

} // namespace crv::pipeline::filters::one_euro
