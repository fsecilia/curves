// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/error_metrics.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/ranges.hpp>
#include <crv/test/float128/float128.hpp>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string_view>

namespace crv {
namespace {

#if defined CRV_FEATURE_FLOAT_128
using reference_float_t = float128_t;
#else
using reference_float_t = float64_t;
#endif

template <typename in_t> struct sweep_range_t
{
    in_t min;
    in_t max;
    in_t step; // step for uniform; max step for fuzz
};

template <typename approximation_t, typename reference_t, typename metrics_t> struct accuracy_test_runner_t
{
    approximation_t approximation;
    reference_t reference;

    using value_t = metrics_t::value_t;
    using clock_t = std::chrono::steady_clock;

    template <typename in_t> auto run_uniform(sweep_range_t<in_t> const& range) const -> void
    {
        std::ostringstream label;
        label << "Uniform Step, Δ = " << range.step.value << " (" << range.step << ")";

        run_sweep(range, label.str(), [&]() { return range.step.value; });
    }

    template <typename in_t>
    auto run_fuzzed(sweep_range_t<in_t> const& range, in_t min_step = in_t::literal(1)) const -> void
    {
        auto const avg_step_val = (range.step.value + min_step.value) / 2;

        auto label = std::ostringstream{};
        label << "Fuzzed Walk, Avg Δ ≈ " << avg_step_val << " (" << in_t::literal(avg_step_val) << ")";

        std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<typename in_t::value_t> step_dist(min_step.value, range.step.value);

        run_sweep(range, label.str(), [&]() { return step_dist(rng); });
    }

private:
    template <typename in_t, typename step_func_t>
    auto run_sweep(sweep_range_t<in_t> const& range, std::string_view label, step_func_t&& get_step) const -> void
    {
        auto const clear_line = "\r\033[2K";
        std::cout << "[" << range.min.value << " (" << range.min << "), " << range.max.value << " (" << range.max
                  << ")], " << label << std::endl;

        metrics_t metrics;

        auto const start_time = clock_t::now();
        auto prev_time = start_time;
        static constexpr auto update_interval = std::chrono::seconds(1);
        static constexpr auto iterations_between_time_checks = 1'000'000;

        auto x_fixed = range.min;

        while (x_fixed < range.max)
        {
            for (auto iteration = 0; iteration < iterations_between_time_checks && x_fixed < range.max; ++iteration)
            {
                auto const x_real = from_fixed<value_t>(x_fixed);
                metrics.sample(x_fixed, approximation(x_fixed), reference(x_real));

                auto const step_val = get_step();

                if (range.max.value - x_fixed.value <= step_val) x_fixed = range.max;
                else x_fixed += in_t::literal(step_val);
            }

            auto const cur_time = clock_t::now();
            if (cur_time - prev_time > update_interval)
            {
                prev_time = cur_time;

                auto const completed = static_cast<value_t>(x_fixed.value) - static_cast<value_t>(range.min.value);
                auto const total = static_cast<value_t>(range.max.value) - static_cast<value_t>(range.min.value);
                auto const elapsed = cur_time - start_time;

                auto remaining = std::chrono::seconds(0);
                if (completed > 0)
                {
                    remaining = std::chrono::duration_cast<std::chrono::seconds>(elapsed * (total / completed - 1));
                }

                std::cout << clear_line << 100 * completed / total << "% (" << remaining.count() << "s remaining)"
                          << std::flush;
            }
        }
        std::cout << clear_line << metrics << "\n" << std::endl;
    }
};

} // namespace
} // namespace crv
