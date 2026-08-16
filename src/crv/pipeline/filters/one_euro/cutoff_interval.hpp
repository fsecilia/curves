// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>
#include <optional>

namespace crv::pipeline::filters::one_euro {

/// calculates dimensionless cutoff interval `cutoff_rate*dt_ns` in a bounded working representation
///
/// Finite results always leave room for the denominator's `+1`:
///
///     0 < interval <= maximum_finite
///
/// An interval larger than the finite working representation is returned as `std::nullopt`. This is not an error;
/// consumers interpret it as the mathematical cutoff-interval limit.
template <is_fixed t_cutoff_interval_t>
    requires(is_signed_v<t_cutoff_interval_t>)
struct cutoff_interval_calculator_t
{
    using cutoff_interval_t = t_cutoff_interval_t;

    /// Largest finite interval for which `interval + 1` is representable.
    static constexpr auto maximum_finite = max<cutoff_interval_t>() - cutoff_interval_t{1};

    /// Forms the dimensionless cutoff interval `cutoff_rate*dt_ns`.
    ///
    /// \returns the finite interval, or `std::nullopt` for the mathematical cutoff-interval limit
    /// \pre cutoff_rate > 0
    /// \pre dt_ns > 0
    template <is_fixed cutoff_rate_t, is_fixed dt_ns_t>
        requires(is_signed_v<cutoff_rate_t> && !is_signed_v<dt_ns_t>)
    constexpr auto calc(cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) const noexcept -> std::optional<cutoff_interval_t>
    {
        using product_t = fixed::product_t<cutoff_rate_t, dt_ns_t>;

        static_assert(cutoff_interval_t::int_bits > 0, "cutoff interval must represent one");
        static_assert(dt_ns_t::frac_bits == 0, "elapsed nanoseconds must use an integer representation");
        static_assert(cutoff_interval_t::frac_bits == cutoff_rate_t::frac_bits,
            "cutoff interval must preserve cutoff-rate precision for positive integer dt");
        static_assert(product_t::int_bits >= cutoff_interval_t::int_bits,
            "cutoff-rate/time product must contain the finite cutoff-interval range");

        assert(cutoff_rate > cutoff_rate_t{});
        assert(dt_ns > dt_ns_t{});

        auto const product = multiply(cutoff_rate, dt_ns);

        // The ceiling comparison must be conservative. maximum_finite can have more fractional precision than
        // product_t, so explicitly truncate it into the product representation rather than allowing a nearest
        // conversion to round the ceiling upward.
        static constexpr auto product_ceiling
            = product_t::template convert<shifter_t<rounding_modes::shr::truncate>{}>(maximum_finite);

        // Converting the truncated ceiling back into the working interval cannot cross maximum_finite. This is the
        // property that guarantees the later denominator addition is safe.
        static_assert(cutoff_interval_t::convert(product_ceiling) <= maximum_finite);

        if (product > product_ceiling) return std::nullopt;

        // dt_ns is a positive integer and the working interval has at least the cutoff rate's fractional precision, so
        // this conversion never discards fractional bits and a positive representable cutoff rate cannot become zero.
        auto const interval = cutoff_interval_t::convert(product);

        assert(interval > cutoff_interval_t{});
        assert(interval <= maximum_finite);

        return interval;
    }
};

} // namespace crv::pipeline::filters::one_euro
