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

namespace crv {

template <typename real_t> struct compensated_accumulator_t;

template <typename arg_t, typename real_t, typename accumulator_t = compensated_accumulator_t<real_t>>
struct accuracy_metrics_t
{
    int_t sample_count = 0;

    struct error_stats_t
    {
        accumulator_t sse{};
        real_t        max{};
        arg_t         arg_max{};

        constexpr auto sample(arg_t arg, real_t error) noexcept -> void
        {
            sse += error * error;
            auto const mag = std::abs(error);
            if (mag > max)
            {
                max     = mag;
                arg_max = arg;
            }
        }

        constexpr auto mse(int_t sample_count) const -> real_t { return sse / sample_count; }
        constexpr auto rmse(int_t sample_count) const -> real_t { return std::sqrt(mse(sample_count)); }

        constexpr auto operator<=>(error_stats_t const&) const noexcept -> auto = default;
        constexpr auto operator==(error_stats_t const&) const noexcept -> bool  = default;
    };

    error_stats_t abs{};
    error_stats_t rel{};

    constexpr auto abs_mse() const -> real_t { return abs.mse(sample_count); }
    constexpr auto abs_rmse() const -> real_t { return abs.rmse(sample_count); }
    constexpr auto rel_mse() const -> real_t { return rel.mse(sample_count); }
    constexpr auto rel_rmse() const -> real_t { return rel.rmse(sample_count); }

    constexpr auto sample(arg_t arg, real_t actual, real_t expected) noexcept -> void
    {
        ++sample_count;

        auto const error = actual - expected;
        abs.sample(arg, error);
        if (std::abs(expected) > epsilon<real_t>()) rel.sample(arg, error / expected);
    }

    constexpr auto operator<=>(accuracy_metrics_t const&) const noexcept -> auto = default;
    constexpr auto operator==(accuracy_metrics_t const&) const noexcept -> bool  = default;
};

} // namespace crv
