// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/error_metrics.hpp>
#include <crv/math/fixed/exp2_neg_m1.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/float128/float128.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace crv {
namespace {

#if defined CRV_FEATURE_FLOAT_128
using reference_float_t = float128_t;
#else
using reference_float_t = float64_t;
#endif

template <typename func_approx_t, typename func_ref_t> struct accuracy_test_t
{
    func_approx_t approx;
    func_ref_t    ref;

    template <typename in_t, typename error_metrics_t>
    auto operator()(error_metrics_t& error_metrics, in_t min, in_t max, in_t delta) const -> void
    {
        auto const clear_line = "\r\033[2K";

        std::cout << "[" << min << ", " << max << "], Δ = " << delta << std::endl;

        using value_t = error_metrics_t::value_t;
        using clock_t = std::chrono::steady_clock;

        auto const            start_time      = clock_t::now();
        auto                  prev_time       = start_time;
        static constexpr auto update_interval = std::chrono::seconds(1);

        auto       x_fixed          = min;
        auto const total_iterations = (max.value - min.value) / delta.value;
        auto       total_iteration  = typename in_t::value_t{0};
        while (total_iteration < total_iterations)
        {
            static constexpr auto iterations_between_time_checks = 1'000'000;
            for (auto iteration = 0; iteration < iterations_between_time_checks && total_iteration < total_iterations;
                 ++iteration, ++total_iteration, x_fixed += delta)
            {
                auto const x_real = from_fixed<value_t>(x_fixed);
                error_metrics.sample(x_fixed, approx(x_fixed), ref(x_real));
            }

            auto const cur_time = clock_t::now();
            if (cur_time - prev_time > update_interval)
            {
                prev_time = cur_time;

                auto const completed = static_cast<value_t>(x_fixed.value) - static_cast<value_t>(min.value);
                auto const total     = static_cast<value_t>(max.value) - static_cast<value_t>(min.value);

                auto const elapsed = cur_time - start_time;
                auto const remaining
                    = std::chrono::duration_cast<std::chrono::seconds>(elapsed * (total / completed - 1));

                std::cout << clear_line << 100 * completed / total << "% (" << remaining << " remaining)" << std::flush;
            }

            ++total_iteration;
            x_fixed += delta;
        }

        std::cout << clear_line << error_metrics << "\n" << std::endl;
    }
};

struct exp2_neg_m1_q64_to_q1_63_test_t
{
    using impl_t = exp2_neg_m1_q64_to_q1_63_t;

    using in_t        = impl_t::in_t;
    using out_t       = impl_t::out_t;
    using reference_t = reference_float_t;

    struct error_metrics_policy_t
        : crv::error_metrics_policy_t<in_t, reference_t, out_t, error_metric::mono_dir_policies::descending_t>
    {};

    auto operator()() noexcept -> void
    {
        using std::log2;

        using metrics_t = error_metrics_t<error_metrics_policy_t>;

        auto const max = crv::max<uint64_t>();

        auto const approx_impl = impl_t{};

        auto const accuracy_test = accuracy_test_t{
            [&](in_t const& x) { return approx_impl.eval(x); },
            [](reference_t const& x) {
                using std::exp2;
                return exp2(-x) - static_cast<reference_t>(1.0);
            },
        };

        auto const iterations  = 10000000ULL;
        auto const coarse_step = max / iterations;

        struct range_t
        {
            in_t min;
            in_t max;
            in_t step_size;
        };

        range_t ranges[] = {
            {0, max / 4, coarse_step},
            {max / 4, max / 2, coarse_step},
            {max / 2, 3 * (max / 4), coarse_step},
            {3 * (max / 4), max, coarse_step},

            {0, max / 2, coarse_step},
            {max / 2, max, coarse_step},

            {0, max, coarse_step},

            {0, iterations, 1},
            {max / 2 - iterations / 2, max / 2 + iterations / 2, 1},
            {max - iterations, max, 1},
        };
        for (auto const& range : ranges)
        {
            metrics_t metrics;
            accuracy_test(metrics, range.min, range.max, range.step_size);
        }
    }
};

auto main(int, char*[]) -> int
{
    exp2_neg_m1_q64_to_q1_63_test_t{}();
    return EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int arg_count, char* args[]) -> int
{
    return crv::main(arg_count, args);
}
