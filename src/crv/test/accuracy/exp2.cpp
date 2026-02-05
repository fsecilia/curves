// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/lib.hpp>
#include <crv/math/accuracy_metrics.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/fixed/conversions.hpp>
#include <crv/math/fixed/exp2.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/test/float128/float128.hpp>
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

template <typename in_t, typename out_t, typename real_t, typename func_approx_t, typename func_ref_t>
void run_accuracy_test(func_approx_t const& approx, func_ref_t const& ref, accuracy_metrics_t<in_t, real_t>& metrics,
                       in_t const& min, in_t const& max, in_t const& delta)
{
    for (auto x_fixed = min; x_fixed <= max; x_fixed += delta)
    {
        auto const x_real = from_fixed<real_t>(x_fixed);
        auto const y_true = ref(x_real);

        auto const y_approx_fixed = approx(x_fixed);
        auto const y_approx_real  = from_fixed<real_t>(y_approx_fixed);

        metrics.sample(x_fixed, y_true, y_approx_real);
    }

    std::cout << "[" << min << ", " << max << "], Δ = " << delta << "\n" << metrics << std::endl;
}

auto test_exp2() noexcept -> void
{
    using std::log2;

    using in_t        = crv::fixed_t<int64_t, 32>;
    using out_t       = crv::fixed_t<uint64_t, 32>;
    using reference_t = reference_float_t;
    using metrics_t   = accuracy_metrics_t<in_t, reference_t>;

    metrics_t metrics;

    auto const max_rep_float = log2(static_cast<reference_t>(max<in_t::value_t>() >> in_t::frac_bits));
    auto const max_rep_int   = static_cast<in_t::value_t>(max_rep_float);
    auto const max           = in_t{max_rep_int << in_t::frac_bits};
    auto const min           = -max;
    auto const iterations    = 1000000;
    auto const delta         = in_t{(max.value - min.value + (iterations / 2)) / iterations};
    auto const approx_impl   = exp2_q32_t{}; // preprod_exp2_t{};
    run_accuracy_test<in_t, out_t, reference_t>(
        [&](in_t const& x) { return approx_impl.template eval<out_t::value_t, out_t::frac_bits>(x); },
        [](reference_t const& x) {
            using std::exp2;
            return exp2(x);
        },
        metrics, min, max, delta);
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
