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
// Arg Max/Min
// --------------------------------------------------------------------------------------------------------------------

template <typename arg_t, typename real_t> struct arg_max_t
{
    real_t max{};
    arg_t  arg_max{};

    constexpr auto sample(arg_t arg, real_t value) noexcept -> void
    {
        if (max < value)
        {
            max     = value;
            arg_max = arg;
        }
    }
};

template <typename arg_t, typename real_t> struct arg_min_t
{
    real_t min{};
    arg_t  arg_min{};

    constexpr auto sample(arg_t arg, real_t value) noexcept -> void
    {
        if (value < min)
        {
            min     = value;
            arg_min = arg;
        }
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Error Stats
// --------------------------------------------------------------------------------------------------------------------

template <typename arg_t, typename real_t, typename accumulator_t = compensated_accumulator_t<real_t>>
struct error_stats_t
{
    int_t         sample_count{};
    accumulator_t sse{};
    real_t        max{};
    arg_t         arg_max{};

    constexpr auto sample(arg_t arg, real_t error) noexcept -> void
    {
        using std::abs;

        ++sample_count;

        sse += error * error;
        auto const mag = abs(error);
        if (mag > max)
        {
            max     = mag;
            arg_max = arg;
        }
    }

    constexpr auto mse() const -> real_t { return sse / static_cast<real_t>(sample_count); }
    constexpr auto rmse() const -> real_t
    {
        using std::sqrt;
        return sqrt(mse());
    }

    constexpr auto operator<=>(error_stats_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_stats_t const&) const noexcept -> bool  = default;

    friend auto operator<<(std::ostream& out, error_stats_t const& src) -> std::ostream&
    {
        return out << "sample count = " << src.sample_count << "\narg_max = " << src.arg_max << "\nmax = " << src.max
                   << "\nmse = " << src.mse() << "\nrmse = " << src.rmse();
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Accuracy Metrics
// --------------------------------------------------------------------------------------------------------------------

template <typename arg_t, typename real_t, typename error_stats_t = error_stats_t<arg_t, real_t>>
struct accuracy_metrics_t
{
    error_stats_t abs{};
    error_stats_t rel{};

    constexpr auto abs_mse() const -> real_t { return abs.mse(); }
    constexpr auto abs_rmse() const -> real_t { return abs.rmse(); }
    constexpr auto rel_mse() const -> real_t { return rel.mse(); }
    constexpr auto rel_rmse() const -> real_t { return rel.rmse(); }

    constexpr auto sample(arg_t arg, real_t actual, real_t expected) noexcept -> void
    {
        auto const error = actual - expected;
        abs.sample(arg, error);
        if (std::abs(expected) > epsilon<real_t>()) rel.sample(arg, error / expected);
    }

    constexpr auto operator<=>(accuracy_metrics_t const&) const noexcept -> auto = default;
    constexpr auto operator==(accuracy_metrics_t const&) const noexcept -> bool  = default;

    friend auto operator<<(std::ostream& out, accuracy_metrics_t const& src) -> std::ostream&
    {
        return out << "\nabs:\n" << src.abs << "\nrel:\n" << src.rel;
    }
};

} // namespace crv
