// SPDX-License-Identifier: MIT
/**
    \file
    \brief statistical error metrics

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/limits.hpp>
#include <cmath>
#include <ostream>

namespace crv {

template <typename real_t> struct compensated_accumulator_t;

// --------------------------------------------------------------------------------------------------------------------
// Min/Max
// --------------------------------------------------------------------------------------------------------------------

/// tracks min@arg
template <typename arg_t, typename real_t> struct arg_min_t
{
    real_t min{max<real_t>()};
    arg_t  arg_min{};

    constexpr auto sample(arg_t arg, real_t value) noexcept -> void
    {
        if (value < min)
        {
            min     = value;
            arg_min = arg;
        }
    }

    friend auto operator<<(std::ostream& out, arg_min_t const& src) -> std::ostream&
    {
        return out << src.min << "@" << src.arg_min;
    }

    constexpr auto operator<=>(arg_min_t const&) const noexcept -> auto = default;
    constexpr auto operator==(arg_min_t const&) const noexcept -> bool  = default;
};

/// tracks max@arg
template <typename arg_t, typename real_t> struct arg_max_t
{
    real_t max{min<real_t>()};
    arg_t  arg_max{};

    constexpr auto sample(arg_t arg, real_t value) noexcept -> void
    {
        if (max < value)
        {
            max     = value;
            arg_max = arg;
        }
    }

    friend auto operator<<(std::ostream& out, arg_max_t const& src) -> std::ostream&
    {
        return out << src.max << "@" << src.arg_max;
    }

    constexpr auto operator<=>(arg_max_t const&) const noexcept -> auto = default;
    constexpr auto operator==(arg_max_t const&) const noexcept -> bool  = default;
};

/// tracks signed min and max, max magnitude
template <typename arg_t, typename real_t, typename arg_min_t = arg_min_t<arg_t, real_t>,
          typename arg_max_t = arg_max_t<arg_t, real_t>>
struct min_max_t
{
    arg_min_t min_signed;
    arg_max_t max_signed;

    constexpr auto max_mag() const noexcept -> real_t
    {
        return std::max(std::abs(min_signed.min), std::abs(max_signed.max));
    }

    constexpr auto arg_max_mag() const noexcept -> arg_t
    {
        return std::abs(min_signed.min) < std::abs(max_signed.max) ? max_signed.arg_max : min_signed.arg_min;
    }

    constexpr auto sample(arg_t arg, real_t value) noexcept -> void
    {
        min_signed.sample(arg, value);
        max_signed.sample(arg, value);
    }

    friend auto operator<<(std::ostream& out, min_max_t const& src) -> std::ostream&
    {
        return out << "min = " << src.min_signed << "\nmax = " << src.max_signed;
    }

    constexpr auto operator<=>(min_max_t const&) const noexcept -> auto = default;
    constexpr auto operator==(min_max_t const&) const noexcept -> bool  = default;
};

// --------------------------------------------------------------------------------------------------------------------
// Error Accumulator
// --------------------------------------------------------------------------------------------------------------------

/// tracks stats about an error metric and provides a summary
template <typename t_arg_t, typename t_real_t, typename accumulator_t = compensated_accumulator_t<t_real_t>,
          typename min_max_t = min_max_t<t_arg_t, t_real_t>>
struct error_accumulator_t
{
    using arg_t  = t_arg_t;
    using real_t = t_real_t;

    accumulator_t sse{};
    accumulator_t sum{};
    min_max_t     min_max{};
    int_t         sample_count{};

    constexpr auto sample(arg_t arg, real_t error) noexcept -> void
    {
        ++sample_count;

        sse += error * error;
        sum += error;
        min_max.sample(arg, error);
    }

    constexpr auto mse() const noexcept -> real_t { return sse / static_cast<real_t>(sample_count); }
    constexpr auto rmse() const noexcept -> real_t
    {
        using std::sqrt;
        return sqrt(mse());
    }

    constexpr auto bias() const noexcept -> real_t { return sum / static_cast<real_t>(sample_count); }

    constexpr auto variance() const noexcept -> real_t
    {
        auto const bias = this->bias();
        return mse() - bias * bias;
    }

    friend auto operator<<(std::ostream& out, error_accumulator_t const& src) -> std::ostream&
    {
        return out << "sample count = " << src.sample_count << "\n"
                   << src.min_max << "\n"
                   << "sum = " << src.sum << "\nmse = " << src.mse() << "\nrmse = " << src.rmse()
                   << "\nbias = " << src.bias() << "\nvariance = " << src.variance();
    }

    constexpr auto operator<=>(error_accumulator_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_accumulator_t const&) const noexcept -> bool  = default;
};

// --------------------------------------------------------------------------------------------------------------------
// Metric Policies
// --------------------------------------------------------------------------------------------------------------------

namespace metric_policy {

struct diff_t
{
    template <typename error_accumulator_t, typename fixed_t>
    constexpr auto operator()(error_accumulator_t& error_accumulator, error_accumulator_t::arg_t arg, fixed_t actual,
                              error_accumulator_t::real_t expected) const noexcept -> void
    {
        auto const diff = from_fixed<typename error_accumulator_t::real_t>(actual) - expected;
        error_accumulator.sample(arg, diff);
    }

    constexpr auto operator<=>(diff_t const&) const noexcept -> auto = default;
    constexpr auto operator==(diff_t const&) const noexcept -> bool  = default;
};

struct rel_t
{
    template <typename error_accumulator_t, typename fixed_t>
    constexpr auto operator()(error_accumulator_t& error_accumulator, error_accumulator_t::arg_t arg, fixed_t actual,
                              error_accumulator_t::real_t expected) const noexcept -> void
    {
        if (std::abs(expected) > epsilon<typename error_accumulator_t::real_t>())
        {
            auto const diff = from_fixed<typename error_accumulator_t::real_t>(actual) - expected;
            auto const rel  = diff / expected;
            error_accumulator.sample(arg, rel);
        }
    }

    constexpr auto operator<=>(rel_t const&) const noexcept -> auto = default;
    constexpr auto operator==(rel_t const&) const noexcept -> bool  = default;
};

struct ulps_t
{
    template <typename error_accumulator_t, typename fixed_t>
    constexpr auto operator()(error_accumulator_t& error_accumulator, error_accumulator_t::arg_t arg, fixed_t actual,
                              error_accumulator_t::real_t expected) const noexcept -> void
    {
        auto const ideal = std::round(std::ldexp(expected, fixed_t::frac_bits));
        auto const ulps  = static_cast<error_accumulator_t::real_t>(actual.value) - ideal;
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
template <typename arg_t, typename real_t, typename policy_t,
          typename error_accumulator_t = error_accumulator_t<arg_t, real_t>>
struct error_metric_t
{
    [[no_unique_address]] policy_t policy;
    error_accumulator_t            error_accumulator;

    template <typename fixed_t> auto sample(arg_t arg, fixed_t actual, real_t expected) noexcept -> void
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

struct default_error_metrics_policy_t
{
    using arg_t  = int_t;
    using real_t = float_t;

    template <typename arg_t, typename real_t>
    using diff_metric_t = error_metric_t<arg_t, real_t, metric_policy::diff_t>;

    template <typename arg_t, typename real_t> using rel_metric_t = error_metric_t<arg_t, real_t, metric_policy::rel_t>;

    template <typename arg_t, typename real_t>
    using ulps_metric_t = error_metric_t<arg_t, real_t, metric_policy::ulps_t>;
};

template <typename policy_t = default_error_metrics_policy_t> struct error_metrics_t
{
    using arg_t         = policy_t::arg_t;
    using real_t        = policy_t::real_t;
    using diff_metric_t = policy_t::template diff_metric_t<arg_t, real_t>;
    using rel_metric_t  = policy_t::template rel_metric_t<arg_t, real_t>;
    using ulps_metric_t = policy_t::template ulps_metric_t<arg_t, real_t>;

    diff_metric_t diff_metric;
    rel_metric_t  rel_metric;
    ulps_metric_t ulps_metric;

    template <typename fixed_t> constexpr auto sample(arg_t arg, fixed_t actual, real_t expected) noexcept -> void
    {
        diff_metric.sample(arg, actual, expected);
        rel_metric.sample(arg, actual, expected);
        ulps_metric.sample(arg, actual, expected);
    }

    friend auto operator<<(std::ostream& out, error_metrics_t const& src) -> std::ostream&
    {
        return out << "diff:\n" << src.diff_metric << "\nrel:\n" << src.rel_metric << "\nulps:\n" << src.ulps_metric;
    }

    constexpr auto operator<=>(error_metrics_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_metrics_t const&) const noexcept -> bool  = default;
};

} // namespace crv
