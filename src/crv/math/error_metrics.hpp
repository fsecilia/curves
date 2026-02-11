// SPDX-License-Identifier: MIT
/**
    \file
    \brief statistical error metrics

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/arg_min_max.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/stats.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <optional>
#include <ostream>

namespace crv {

template <typename error_accumulator_t>
concept float_error_accumulator = std::floating_point<typename error_accumulator_t::value_t>;

template <typename error_accumulator_t>
concept int_error_accumulator = std::signed_integral<typename error_accumulator_t::value_t>;

// --------------------------------------------------------------------------------------------------------------------
// Faithfully-Rounded Fraction
// --------------------------------------------------------------------------------------------------------------------

// fr_frac is the fraction of faithfully-rounded samples that are within 1-ulp of the expected result.
template <typename float_t> struct fr_frac_t
{
    int_t faithfully_rounded_count{};
    int_t sample_count{};

    auto result() const noexcept -> float_t
    {
        return sample_count ? static_cast<float_t>(faithfully_rounded_count) / static_cast<float_t>(sample_count) : 0;
    }

    auto sample(int_t ulps) noexcept -> void
    {
        ++sample_count;

        using std::abs;
        if (abs(ulps) <= 1) ++faithfully_rounded_count;
    }

    friend auto operator<<(std::ostream& out, fr_frac_t const& src) -> std::ostream& { return out << src.result(); }

    constexpr auto operator<=>(fr_frac_t const&) const noexcept -> auto = default;
    constexpr auto operator==(fr_frac_t const&) const noexcept -> bool  = default;
};

// --------------------------------------------------------------------------------------------------------------------
// Error Accumulators
// --------------------------------------------------------------------------------------------------------------------

template <typename arg_t, typename float_t, typename error_accumulator_t = error_accumulator_t<arg_t, float_t>,
          typename distribution_t = distribution_t<int_t>, typename fr_frac_t = fr_frac_t<float_t>>
struct ulps_error_accumulator_t : error_accumulator_t
{
    using value_t = int_t;

    distribution_t distribution{};
    fr_frac_t      fr_frac{};

    constexpr auto sample(arg_t arg, value_t error) noexcept -> void
    {
        error_accumulator_t::sample(arg, static_cast<float_t>(error));
        distribution.sample(error);
        fr_frac.sample(error);
    }

    friend auto operator<<(std::ostream& out, ulps_error_accumulator_t const& src) -> std::ostream&
    {
        return out << static_cast<error_accumulator_t const&>(src) << "\n"
                   << src.distribution << "\nfr_frac = " << src.fr_frac;
    }

    constexpr auto operator<=>(ulps_error_accumulator_t const&) const noexcept -> auto = default;
    constexpr auto operator==(ulps_error_accumulator_t const&) const noexcept -> bool  = default;
};

/// tracks monotonicity per sample
template <typename arg_t, typename value_t, typename error_accumulator_t = error_accumulator_t<arg_t, value_t>>
struct mono_error_accumulator_t : error_accumulator_t
{
    std::optional<value_t> prev{};
    int_t                  violation_count{};

    template <typename fixed_t> constexpr auto sample(arg_t arg, fixed_t actual) noexcept -> void
    {
        auto const cur = from_fixed<value_t>(actual);
        if (!prev)
        {
            prev = cur;
            return;
        }

        auto const error = std::max(value_t{0}, cur - *prev);
        *prev            = cur;

        if (error != 0) ++violation_count;

        error_accumulator_t::sample(arg, error);
    }

    friend auto operator<<(std::ostream& out, mono_error_accumulator_t const& src) -> std::ostream&
    {
        out << "violation count = " << src.violation_count;
        if (!src.violation_count) return out;

        return out << "\n" << static_cast<error_accumulator_t const&>(src);
    }

    constexpr auto operator<=>(mono_error_accumulator_t const&) const noexcept -> auto = default;
    constexpr auto operator==(mono_error_accumulator_t const&) const noexcept -> bool  = default;
};

// --------------------------------------------------------------------------------------------------------------------
// Metric Policies
// --------------------------------------------------------------------------------------------------------------------
// These define how specific categories of error are calculated.

namespace metric_policy {

/// calcs signed diff
struct diff_t
{
    template <float_error_accumulator error_accumulator_t, typename arg_t, typename fixed_t, typename value_t>
    constexpr auto operator()(error_accumulator_t& error_accumulator, arg_t arg, fixed_t actual,
                              value_t expected) const noexcept -> void
    {
        auto const diff = from_fixed<value_t>(actual) - expected;
        error_accumulator.sample(arg, diff);
    }

    constexpr auto operator<=>(diff_t const&) const noexcept -> auto = default;
    constexpr auto operator==(diff_t const&) const noexcept -> bool  = default;
};

/**
    calcs signed relative diff, diff/expected

    Samples with expected value of 0 are omitted.
*/
struct rel_t
{
    template <float_error_accumulator error_accumulator_t, typename arg_t, typename fixed_t, typename value_t>
    constexpr auto operator()(error_accumulator_t& error_accumulator, arg_t arg, fixed_t actual,
                              value_t expected) const noexcept -> void
    {
        if (std::abs(expected) > epsilon<value_t>())
        {
            auto const diff = from_fixed<value_t>(actual) - expected;
            auto const rel  = diff / expected;
            error_accumulator.sample(arg, rel);
        }
    }

    constexpr auto operator<=>(rel_t const&) const noexcept -> auto = default;
    constexpr auto operator==(rel_t const&) const noexcept -> bool  = default;
};

/// calcs signed ulps, units-in-last-place
struct ulps_t
{
    template <int_error_accumulator error_accumulator_t, typename arg_t, typename fixed_t, typename value_t>
    constexpr auto operator()(error_accumulator_t& error_accumulator, arg_t arg, fixed_t actual,
                              value_t expected) const noexcept -> void
    {
        using int_value_t = error_accumulator_t::value_t;
        auto const ideal  = static_cast<int_value_t>(std::round(std::ldexp(expected, fixed_t::frac_bits)));
        auto const ulps   = actual.value - ideal;
        error_accumulator.sample(arg, ulps);
    }

    constexpr auto operator<=>(ulps_t const&) const noexcept -> auto = default;
    constexpr auto operator==(ulps_t const&) const noexcept -> bool  = default;
};

} // namespace metric_policy

// --------------------------------------------------------------------------------------------------------------------
// Error Metric
// --------------------------------------------------------------------------------------------------------------------

/// associates an error accumulator with a metric policy to actually calc the specific type of error
template <typename arg_t, typename value_t, typename policy_t,
          typename error_accumulator_t = error_accumulator_t<arg_t, value_t>>
struct error_metric_t
{
    [[no_unique_address]] policy_t policy;
    error_accumulator_t            error_accumulator;

    template <typename fixed_t> constexpr auto sample(arg_t arg, fixed_t actual, value_t expected) noexcept -> void
    {
        policy(error_accumulator, arg, actual, expected);
    }

    friend auto operator<<(std::ostream& out, error_metric_t const& src) -> std::ostream&
    {
        return out << src.error_accumulator;
    }

    constexpr auto operator<=>(error_metric_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_metric_t const&) const noexcept -> bool  = default;
};

// --------------------------------------------------------------------------------------------------------------------
// Error Metrics
// --------------------------------------------------------------------------------------------------------------------

/// default policy used in prod
struct default_error_metrics_policy_t
{
    using arg_t   = int_t;
    using value_t = float_t;

    template <typename arg_t, typename value_t>
    using diff_metric_t = error_metric_t<arg_t, value_t, metric_policy::diff_t>;

    template <typename arg_t, typename value_t>
    using rel_metric_t = error_metric_t<arg_t, value_t, metric_policy::rel_t>;

    template <typename arg_t, typename value_t>
    using ulps_metric_t
        = error_metric_t<arg_t, value_t, metric_policy::ulps_t, ulps_error_accumulator_t<arg_t, value_t>>;

    template <typename arg_t, typename value_t>
    using mono_error_accumulator_t = mono_error_accumulator_t<arg_t, value_t>;
};

/// collects metrics about various types of error
template <typename policy_t = default_error_metrics_policy_t> struct error_metrics_t
{
    using arg_t                    = policy_t::arg_t;
    using value_t                  = policy_t::value_t;
    using diff_metric_t            = policy_t::template diff_metric_t<arg_t, value_t>;
    using rel_metric_t             = policy_t::template rel_metric_t<arg_t, value_t>;
    using ulps_metric_t            = policy_t::template ulps_metric_t<arg_t, value_t>;
    using mono_error_accumulator_t = policy_t::template mono_error_accumulator_t<arg_t, value_t>;

    diff_metric_t            diff_metric;
    rel_metric_t             rel_metric;
    ulps_metric_t            ulps_metric;
    mono_error_accumulator_t mono_error_accumulator;

    template <typename fixed_t> constexpr auto sample(arg_t arg, fixed_t actual, value_t expected) noexcept -> void
    {
        diff_metric.sample(arg, actual, expected);
        rel_metric.sample(arg, actual, expected);
        ulps_metric.sample(arg, actual, expected);
        mono_error_accumulator.sample(arg, actual);
    }

    friend auto operator<<(std::ostream& out, error_metrics_t const& src) -> std::ostream&
    {
        // clang-format off
        return out
            << "diff:\n" << src.diff_metric
            << "\nrel:\n" << src.rel_metric
            << "\nulps:\n" << src.ulps_metric
            << "\nmono:\n" << src.mono_error_accumulator;
        // clang-format on
    }

    constexpr auto operator<=>(error_metrics_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_metrics_t const&) const noexcept -> bool  = default;
};

} // namespace crv
