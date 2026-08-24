// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/model/composed_curve.hpp>
#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/construction/spline/amr/spline_generator.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/accuracy/accuracy_test_runner.hpp>
#include <cmath>
#include <cstdlib>

namespace crv {
namespace {

struct curves_test_t
{
    using pipeline_config_t = spline::prod_pipeline_config_t;
    using spline_factory_policy_t = spline::default_spline_policy_t<reference_float_t, pipeline_config_t>;
    using spline_factory_t = spline::spline_factory_t<spline_factory_policy_t,
        spline::spline_generator_factory_t<spline_factory_policy_t>>;

    using in_t = pipeline_config_t::x_t;
    using out_t = pipeline_config_t::y_t;
    using impl_t = spline_factory_t::spline_t;
    using error_metrics_t = error_metrics_t<error_metrics_policy_t<in_t, reference_float_t, out_t>>;

    struct approximation_t
    {
        impl_t const* spline;
        mutable impl_t::hint_t hint{};

        auto operator()(in_t x) const noexcept -> out_t { return spline->evaluate(x, hint); }
    };

    auto operator()() noexcept -> void
    {
        using range_t = sweep_range_t<in_t>;

        auto curve_config = model::curves::synchronous_t::config_t{};
        curve_config.motivity.value(21.0);
        curve_config.gamma.value(1.25);
        curve_config.smooth.value(0.25);
        curve_config.sync_speed.value(0.75);
        auto const curve = model::curves::create_composed_curve<reference_float_t>(curve_config);
        auto const target = spline::gain_curve_target_t{curve};
        auto const ref_impl = [&target](auto x) { return target.transfer(x); };

        auto approx_impl = impl_t{};
        spline_factory_t{}(approx_impl, target, reference_float_t{1e-10});

        auto const approximation = approximation_t{.spline = &approx_impl};
        auto const runner = accuracy_test_runner_t<decltype(approximation), decltype(ref_impl), error_metrics_t>{
            approximation, ref_impl};

        auto const min = in_t{};
        auto const max = in_t{spline_factory_policy_t::domain_end};

        auto const sync_speed = to_fixed<in_t>(curve_config.sync_speed.value());
        auto const half = min + (max - min) / 2;

        auto const iterations = 10'000'000ULL;
        auto const coarse_step = (max - min) / iterations;
        auto const dense_step = in_t::literal(1);
        auto const dense_range = in_t::literal(iterations);

        range_t uniform_ranges[] = {
            // coarse sweeps
            {min, max / 4, coarse_step},
            {max / 4, max / 2, coarse_step},
            {max / 2, 3 * (max / 4), coarse_step},
            {3 * (max / 4), max, coarse_step},

            // dense uniform sweeps
            {min, dense_range, dense_step},
            {sync_speed - dense_range / 2, sync_speed + dense_range / 2, dense_step},
            {half - dense_range / 2, half + dense_range / 2, dense_step},
            {max - dense_range, dense_range, dense_step},
        };

        for (auto const& range : uniform_ranges) { runner.run_uniform(range); }

        range_t fuzzed_ranges[] = {{min, max, in_t::literal(coarse_step.value * 2)}};

        for (auto const& range : fuzzed_ranges) { runner.run_fuzzed(range); }
    }
};

auto main(int, char*[]) -> int
{
    curves_test_t{}();
    return EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int arg_count, char* args[]) -> int
{
    return crv::main(arg_count, args);
}
