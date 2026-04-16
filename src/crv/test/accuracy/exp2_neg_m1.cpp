// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/error_metrics.hpp>
#include <crv/math/fixed/exp2_neg_m1.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/ranges.hpp>
#include <crv/test/float128/float128.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
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

template <typename approximation_t, typename reference_t, typename metrics_t> struct accuracy_runner_t
{
    approximation_t approximation;
    reference_t     reference;

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

        std::mt19937_64                                       rng(std::random_device{}());
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

        auto const            start_time                     = clock_t::now();
        auto                  prev_time                      = start_time;
        static constexpr auto update_interval                = std::chrono::seconds(1);
        static constexpr auto iterations_between_time_checks = 1'000'000;

        auto x_fixed = range.min;

        while (x_fixed < range.max)
        {
            for (auto iteration = 0; iteration < iterations_between_time_checks && x_fixed < range.max; ++iteration)
            {
                auto const x_real = from_fixed<value_t>(x_fixed);
                metrics.sample(x_fixed, approximation(x_fixed), reference(x_real));

                auto const step_val = get_step();

                if (range.max.value - x_fixed.value <= step_val) { x_fixed = range.max; }
                else
                {
                    x_fixed += in_t::literal(step_val);
                }
            }

            auto const cur_time = clock_t::now();
            if (cur_time - prev_time > update_interval)
            {
                prev_time = cur_time;

                auto const completed = static_cast<value_t>(x_fixed.value) - static_cast<value_t>(range.min.value);
                auto const total     = static_cast<value_t>(range.max.value) - static_cast<value_t>(range.min.value);
                auto const elapsed   = cur_time - start_time;

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

struct rexp2_m1_test_t
{
    using impl_t      = exp2_neg_m1_q64_to_q1_63_t;
    using in_t        = impl_t::in_t;
    using out_t       = impl_t::out_t;
    using reference_t = reference_float_t;

    using error_metrics_t = error_metrics_t<
        error_metrics_policy_t<in_t, reference_t, out_t, error_metric::mono_dir_policies::descending_t>>;

    auto operator()() noexcept -> void
    {
        using range_t      = sweep_range_t<in_t>;
        auto const max_val = crv::max<uint64_t>();
        auto const max     = in_t::literal(max_val);

        auto const approx_impl = impl_t{};
        auto const ref_impl    = [](reference_t const& x) { return std::exp2(-x) - static_cast<reference_t>(1.0); };

        auto const runner
            = accuracy_runner_t<decltype(approx_impl), decltype(ref_impl), error_metrics_t>{approx_impl, ref_impl};

        auto const iterations  = 10'000'000ull;
        auto const coarse_step = in_t::literal(max_val / iterations);

        range_t uniform_ranges[] = {
            // coarse sweeps
            {in_t::literal(0), in_t::literal(max_val / 4), coarse_step},
            {in_t::literal(max_val / 4), in_t::literal(max_val / 2), coarse_step},
            {in_t::literal(max_val / 2), in_t::literal(3 * (max_val / 4)), coarse_step},
            {in_t::literal(3 * (max_val / 4)), max, coarse_step},
            {in_t::literal(0), max, coarse_step},

            // dense sweeps
            {in_t::literal(0), in_t::literal(iterations), in_t::literal(1)},
            {
                in_t::literal(max_val / 2 - iterations / 2),
                in_t::literal(max_val / 2 + iterations / 2),
                in_t::literal(1),
            },
            {in_t::literal(max_val - iterations), max, in_t::literal(1)},
        };

        for (auto const& range : uniform_ranges) { runner.run_uniform(range); }

        range_t fuzzed_ranges[] = {
            // fuzzed random walk sweep across the entire domain targeting ~iterations samples
            {in_t::literal(0), max, in_t::literal(coarse_step.value * 2)},
        };

        for (auto const& range : fuzzed_ranges) { runner.run_fuzzed(range); }
    }
};

auto main(int, char*[]) -> int
{
    rexp2_m1_test_t{}();
    return EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int arg_count, char* args[]) -> int
{
    return crv::main(arg_count, args);
}
