// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <iomanip>
#include <stdfloat>
#include <vector>

namespace crv::spline {
namespace {

TEST(spline_factory_integration_test, evaluates_spline_within_tolerance)
{
#if 0
    using scalar_t = std::float128_t;
#else
    using scalar_t = float_t;
#endif

    using policy_t = default_spline_policy_t<scalar_t, prod_pipeline_config_t>;
    using x_t = policy_t::x_t;

    constexpr auto global_tolerance = scalar_t{1e-10};
    constexpr auto domain_end = policy_t::domain_end;

    auto const target_function = [](auto x) static noexcept -> decltype(x) {
        using std::log1p;
        return 2.1 * log1p(x);
    };

    using spline_factory_t = spline_factory_t<policy_t, spline_generator_factory_t<policy_t>>;
    using spline_t = spline_factory_t::spline_t;
    auto spline = spline_t{};
    spline_factory_t{}(spline, target_function, global_tolerance, {x_t{1 << 3}, x_t{1 << 5}, to_fixed<x_t>(248.973)});

    auto x_fixed = x_t{0};
    auto const sample_count = 255;
    auto const dx = x_t{domain_end} / sample_count;

    for (auto sample = 0; sample < sample_count; ++sample, x_fixed += dx)
    {
        auto const x_real = from_fixed<scalar_t>(x_fixed);

        auto const expected_y = target_function(x_real);
        auto const actual_y = from_fixed<scalar_t>(spline(x_fixed));

        auto const difference = actual_y - expected_y;

        ASSERT_LT(abs(difference), 8.0e-7);

        std::cout << std::setprecision(4) << "x = " << static_cast<float_max_t>(x_real)
                  << ", f(x) = " << static_cast<float_max_t>(expected_y)
                  << ", y_actual = " << static_cast<float_max_t>(actual_y)
                  << ", Δy = " << static_cast<float_max_t>(difference) << std::endl;
    }
    std::cout << std::endl;
}

} // namespace
} // namespace crv::spline
