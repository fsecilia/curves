// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/lib.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/error_metrics.hpp>
#include <crv/math/fixed/conversions.hpp>
#include <crv/math/fixed/exp2.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
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

template <typename in_t, typename func_approx_t, typename func_ref_t> struct accuracy_test_t
{
    in_t          domain_min;
    in_t          domain_max;
    func_approx_t approx;
    func_ref_t    ref;

    template <typename error_metrics_t>
    auto operator()(error_metrics_t& error_metrics, in_t min, in_t max, in_t delta) const -> void
    {
        auto const clear_line = "\r\033[2K";

        std::cout << "[" << min << ", " << max << "], Δ = " << delta << std::endl;

        using value_t = error_metrics_t::value_t;
        using clock_t = std::chrono::steady_clock;

        auto const            start_time      = clock_t::now();
        auto                  prev_time       = start_time;
        static constexpr auto update_interval = std::chrono::seconds(1);

        for (auto x_fixed = min; x_fixed <= max; x_fixed += delta)
        {
            static constexpr auto iterations_between_time_checks = 1'000'000;
            for (auto iteration = 0; iteration < iterations_between_time_checks && x_fixed <= max;
                 ++iteration, x_fixed += delta)
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
        }

        std::cout << clear_line << error_metrics << "\n" << std::endl;
    }
};

auto test_exp2() noexcept -> void
{
    using std::log2;

    using in_t        = crv::fixed_t<int64_t, 32>;
    using out_t       = crv::fixed_t<uint64_t, 32>;
    using reference_t = reference_float_t;

    struct error_metrics_policy_t : default_error_metrics_policy_t
    {
        using arg_t  = in_t;
        using value_t = reference_t;
    };
    using metrics_t = error_metrics_t<error_metrics_policy_t>;

    auto const max_rep_float = log2(static_cast<reference_t>(max<in_t::value_t>() >> in_t::frac_bits));
    auto const max_rep_int   = static_cast<in_t::value_t>(max_rep_float);
    auto const max           = max_rep_int << in_t::frac_bits;
    auto const min           = -max;

    // auto const approx_impl = exp2_q32_t{};
    auto const approx_impl = preprod_exp2_t{};

    auto const accuracy_test = accuracy_test_t{
        in_t{min},
        in_t{max},
        [&](in_t const& x) { return approx_impl.template eval<out_t::value_t, out_t::frac_bits>(x); },
        [](reference_t const& x) {
            using std::exp2;
            return exp2(x);
        },
    };

    auto const iterations  = 1000000;
    auto const coarse_step = (max - min + iterations / 2) / iterations;

    struct range_t
    {
        in_t min;
        in_t max;
        in_t step_size;
    };

    range_t ranges[] = {
        {min, 0, coarse_step},
        {min / 2, 0, coarse_step},
        {min / 2, max / 2, coarse_step},
        {0, max / 2, coarse_step},
        {0, max, coarse_step},
        {min, max, coarse_step},
        {min, in_t{min} + to_fixed<in_t>(0.005), 1},
        {to_fixed<in_t>(-0.5), to_fixed<in_t>(-0.495), 1},
        {to_fixed<in_t>(-0.005), to_fixed<in_t>(0.005), 1},
        {to_fixed<in_t>(0.495), to_fixed<in_t>(0.5), 1},
        {in_t{max} - to_fixed<in_t>(0.005), max, 1},
    };
    for (auto const& range : ranges)
    {
        metrics_t metrics;
        accuracy_test(metrics, range.min, range.max, range.step_size);
    }
}

auto main(int, char*[]) -> int
{
    test_exp2();
    return EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int arg_count, char* args[]) -> int
{
    return crv::main(arg_count, args);
}
