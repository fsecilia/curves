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


    friend auto operator<<(std::ostream& out, error_stats_t const& src) -> std::ostream&
    {
        return out << "sample count = " << src.sample_count << "\narg_max = " << src.arg_max << "\nmax = " << src.max
                   << "\nmse = " << src.mse() << "\nrmse = " << src.rmse();
    }

    constexpr auto operator<=>(error_stats_t const&) const noexcept -> auto = default;
    constexpr auto operator==(error_stats_t const&) const noexcept -> bool  = default;
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

    friend auto operator<<(std::ostream& out, accuracy_metrics_t const& src) -> std::ostream&
    {
        return out << "abs:\n" << src.abs << "\nrel:\n" << src.rel;
    }

    constexpr auto operator<=>(accuracy_metrics_t const&) const noexcept -> auto = default;
    constexpr auto operator==(accuracy_metrics_t const&) const noexcept -> bool  = default;
};

} // namespace crv
