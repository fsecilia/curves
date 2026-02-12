// SPDX-License-Identifier: MIT
/**
    \file
    \brief statistical error metrics

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/arg_min_max.hpp>
#include <crv/math/fixed/fixed.hpp>
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
// Individual Error Metrics
// --------------------------------------------------------------------------------------------------------------------

namespace error_metric {

/// tracks signed diff
template <typename arg_t, typename value_t, typename error_accumulator_t = stats_accumulator_t<arg_t, value_t>>
struct diff_t
{
    error_accumulator_t error_accumulator{};

    constexpr auto sample(arg_t arg, value_t actual, value_t expected) noexcept -> void
    {
        error_accumulator.sample(arg, actual - expected);
    }

    friend auto operator<<(std::ostream& out, diff_t const& src) -> std::ostream&
    {
        return out << src.error_accumulator;
    }

    constexpr auto operator<=>(diff_t const&) const noexcept -> auto = default;
    constexpr auto operator==(diff_t const&) const noexcept -> bool  = default;
};

/**
    tracks signed relative diff, diff/expected

    Samples with expected value of 0 are omitted.
*/
template <typename arg_t, typename value_t, typename error_accumulator_t = stats_accumulator_t<arg_t, value_t>>
struct rel_t
{
    error_accumulator_t error_accumulator{};

    constexpr auto sample(arg_t arg, value_t actual, value_t expected) noexcept -> void
    {
        if (std::abs(expected) <= epsilon<value_t>()) return;
        error_accumulator.sample(arg, (actual - expected) / expected);
    }

    friend auto operator<<(std::ostream& out, rel_t const& src) -> std::ostream&
    {
        return out << src.error_accumulator;
    }

    constexpr auto operator<=>(rel_t const&) const noexcept -> auto = default;
    constexpr auto operator==(rel_t const&) const noexcept -> bool  = default;
};

/// tracks signed ulps
template <typename arg_t, typename value_t, typename fixed_t,
          typename error_accumulator_t = stats_accumulator_t<arg_t, value_t>,
          typename distribution_t = distribution_t<int_t>, typename fr_frac_t = fr_frac_t<value_t>>
struct ulps_t
{
    error_accumulator_t error_accumulator{};
    distribution_t      distribution{};
    fr_frac_t           fr_frac{};

    constexpr auto sample(arg_t arg, fixed_t actual, value_t expected) noexcept -> void
    {
        auto const ulps
            = actual.value - static_cast<fixed_t::value_t>(std::round(std::ldexp(expected, fixed_t::frac_bits)));
        error_accumulator.sample(arg, ulps);
        distribution.sample(ulps);
        fr_frac.sample(ulps);
    }

    friend auto operator<<(std::ostream& out, ulps_t const& src) -> std::ostream&
    {
        return out << src.error_accumulator << "\n" << src.distribution << "\nfr_frac = " << src.fr_frac;
    }

    constexpr auto operator<=>(ulps_t const&) const noexcept -> auto = default;
    constexpr auto operator==(ulps_t const&) const noexcept -> bool  = default;
};

/// tracks monotonicity per sample
template <typename arg_t, typename value_t, typename fixed_t,
          typename error_accumulator_t = stats_accumulator_t<arg_t, value_t>>
struct mono_t
{
    error_accumulator_t    error_accumulator{};
    std::optional<fixed_t> prev{};
    int_t                  violation_count{};

    constexpr auto sample(arg_t arg, fixed_t actual) noexcept -> void
    {
        if (!prev)
        {
            prev = actual;
            return;
        }

        auto const error = actual - *prev;
        *prev            = actual;

        if (error < 0) ++violation_count;

        error_accumulator.sample(arg, from_fixed<value_t>(std::max(fixed_t{0}, error)));
    }

    constexpr auto violation_frac() const noexcept -> value_t
    {
        auto const sample_count = error_accumulator.sample_count;
        assert(sample_count);
        return static_cast<value_t>(violation_count) / sample_count;
    }

    friend auto operator<<(std::ostream& out, mono_t const& src) -> std::ostream&
    {
        out << "violations = " << src.violation_count;
        if (!src.violation_count) return out << " (0%)";

        return out << " (" << src.violation_frac() * 100 << "%)\n" << src.error_accumulator;
    }

    constexpr auto operator<=>(mono_t const&) const noexcept -> auto = default;
    constexpr auto operator==(mono_t const&) const noexcept -> bool  = default;
};

} // namespace error_metric

// --------------------------------------------------------------------------------------------------------------------
// Error Accumulators
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
// Error Metric
// --------------------------------------------------------------------------------------------------------------------

/// associates an error accumulator with a metric policy to actually calc the specific type of error
template <typename arg_t, typename value_t, typename policy_t,
          typename error_accumulator_t = stats_accumulator_t<arg_t, value_t>>
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
    using fixed_t = fixed_t<int_t, 32>;

    template <typename arg_t, typename value_t> using diff_metric_t = error_metric::diff_t<arg_t, value_t>;
    template <typename arg_t, typename value_t> using rel_metric_t  = error_metric::rel_t<arg_t, value_t>;

    template <typename arg_t, typename value_t, typename fixed_t>
    using ulps_metric_t = error_metric::ulps_t<arg_t, value_t, fixed_t>;

    template <typename arg_t, typename value_t, typename fixed_t>
    using mono_metric_t = error_metric::mono_t<arg_t, value_t, fixed_t>;
};

/// collects metrics about various types of error
template <typename policy_t = default_error_metrics_policy_t> struct error_metrics_t
{
    using arg_t         = policy_t::arg_t;
    using value_t       = policy_t::value_t;
    using fixed_t       = policy_t::fixed_t;
    using diff_metric_t = policy_t::template diff_metric_t<arg_t, value_t>;
    using rel_metric_t  = policy_t::template rel_metric_t<arg_t, value_t>;
    using ulps_metric_t = policy_t::template ulps_metric_t<arg_t, value_t, fixed_t>;
    using mono_metric_t = policy_t::template mono_metric_t<arg_t, value_t, fixed_t>;

    diff_metric_t diff_metric;
    rel_metric_t  rel_metric;
    ulps_metric_t ulps_metric;
    mono_metric_t mono_metric;

    template <typename fixed_t>
    constexpr auto sample(arg_t arg, fixed_t actual_fixed, value_t expected) noexcept -> void
    {
        auto const actual_value = from_fixed<value_t>(actual_fixed);
        diff_metric.sample(arg, actual_value, expected);
        rel_metric.sample(arg, actual_value, expected);
        ulps_metric.sample(arg, actual_fixed, expected);
        mono_metric.sample(arg, actual_fixed);
    }

    friend auto operator<<(std::ostream& out, error_metrics_t const& src) -> std::ostream&
    {
        // clang-format off
        return out
            << "diff:\n" << src.diff_metric
            << "\nrel:\n" << src.rel_metric
            << "\nulps:\n" << src.ulps_metric
            << "\nmono:\n" << src.mono_metric;
        // clang-format on
    }

    constexpr auto operator<=>(error_metrics_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_metrics_t const&) const noexcept -> bool  = default;
};

} // namespace crv
