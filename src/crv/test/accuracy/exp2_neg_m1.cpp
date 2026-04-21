// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/fixed/exp2_neg_m1.hpp>
#include <crv/test/accuracy/accuracy_test_runner.hpp>
#include <cmath>
#include <cstdlib>

namespace crv {
namespace {

struct rexp2_m1_test_t
{
    using impl_t = exp2_neg_m1_q64_to_q1_63_t;
    using in_t = impl_t::in_t;
    using out_t = impl_t::out_t;
    using reference_t = reference_float_t;

    using error_metrics_t = error_metrics_t<
        error_metrics_policy_t<in_t, reference_t, out_t, error_metric::mono_dir_policies::descending_t>>;

    auto operator()() noexcept -> void
    {
        using range_t = sweep_range_t<in_t>;
        auto const max_val = crv::max<uint64_t>();
        auto const max = in_t::literal(max_val);

        auto const approx_impl = impl_t{};
        auto const ref_impl = [](reference_t const& x) { return std::exp2(-x) - static_cast<reference_t>(1.0); };

        auto const runner
            = accuracy_test_runner_t<decltype(approx_impl), decltype(ref_impl), error_metrics_t>{approx_impl, ref_impl};

        auto const iterations = 10'000'000ull;
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
