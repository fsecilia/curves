// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/fixed/exp2.hpp>
#include <crv/test/accuracy/accuracy_test_runner.hpp>
#include <cmath>
#include <cstdlib>

namespace crv {
namespace {

struct exp2_test_t
{
    using impl_t      = exp2_normalized_q64_to_q1_63_t;
    using in_t        = impl_t::in_t;
    using out_t       = impl_t::out_t;
    using reference_t = reference_float_t;

    using error_metrics_t = error_metrics_t<
        error_metrics_policy_t<in_t, reference_t, out_t, error_metric::mono_dir_policies::ascending_t>>;

    auto operator()() noexcept -> void
    {
        using range_t = sweep_range_t<in_t>;

        auto const approx_impl = impl_t{};
        auto const ref_impl    = [](reference_t const& x) { return std::exp2(x); };

        auto const runner
            = accuracy_test_runner_t<decltype(approx_impl), decltype(ref_impl), error_metrics_t>{approx_impl, ref_impl};

        auto const max_val  = crv::max<uint64_t>();
        auto const half_val = 1ULL << (in_t::frac_bits - 1);
        auto const qtr_val  = half_val / 2;

        auto const min       = in_t::literal(0);
        auto const max       = in_t::literal(max_val);
        auto const qtr       = in_t::literal(qtr_val);
        auto const half      = in_t::literal(half_val);
        auto const three_qtr = in_t::literal(half_val + qtr_val);

        auto const iterations  = 10'000'000ull;
        auto const coarse_step = in_t::literal(max_val / iterations);

        range_t uniform_ranges[] = {
            // Quadrants
            {min, qtr, coarse_step},
            {qtr, half, coarse_step},
            {half, three_qtr, coarse_step},
            {three_qtr, max, coarse_step},
            // Full span
            {min, max, coarse_step},

            // Dense uniform sweeps target the reduction boundaries
            {min, in_t::literal(iterations), in_t::literal(1)},          // Near 0.0
            {in_t::literal(half_val - iterations / 2), in_t::literal(half_val + iterations / 2),
             in_t::literal(1)},                                          // The 0.5 internal reduction threshold
            {in_t::literal(max_val - iterations), max, in_t::literal(1)} // Approaching 1.0
        };

        for (auto const& range : uniform_ranges) { runner.run_uniform(range); }

        range_t fuzzed_ranges[] = {{min, max, in_t::literal(coarse_step.value * 2)}};

        for (auto const& range : fuzzed_ranges) { runner.run_fuzzed(range); }
    }
};

auto main(int, char*[]) -> int
{
    exp2_test_t{}();
    return EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int arg_count, char* args[]) -> int
{
    return crv::main(arg_count, args);
}
