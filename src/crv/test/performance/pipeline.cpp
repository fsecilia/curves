// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/rsqrt.hpp>
#include <crv/math/linear.hpp>
#include <crv/test/performance/performance.hpp>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace crv {
namespace {

struct pipeline_t
{
    using mouse_dim_t = int16_t;
    using mouse_vector_t = math::vector_t<mouse_dim_t, 2>;
    using length_squared_t = fixed_t<uint32_t, 0>;
    using length_t = fixed_t<uint64_t, 62>;
    using curve_input_t = fixed_t<uint64_t, 56>;
    using rsqrt_t = rsqrt_t<length_t, length_squared_t>;

    rsqrt_t rsqrt;

    auto run(mouse_vector_t in) -> mouse_vector_t
    {
        auto const length_squared = rsqrt_t::in_t::literal(in[0] * in[0] + in[1] * in[1]);
        auto const rlength = rsqrt(length_squared);
        auto const length = length_t::convert(length_squared * rlength);

        do_not_optimize(length);

        return in;
    }
};

struct test_case_t
{
    pipeline_t::mouse_vector_t in;
};

/// pre-generates randomized inputs to keep generation latency out of the benchmark loop
auto generate_test_data(size_t sample_size) -> std::vector<test_case_t>
{
    auto data = std::vector<test_case_t>{};
    data.reserve(sample_size);

    auto rng = std::mt19937_64(std::random_device{}());

    auto dim_dist = std::uniform_int_distribution<pipeline_t::mouse_dim_t>{0, max<pipeline_t::mouse_dim_t>()};

    for (size_t index = 0; index < sample_size; ++index) { data.push_back({dim_dist(rng), dim_dist(rng)}); }

    return data;
}

/// executes the microbenchmark on a generic callable
template <typename invocable_t>
auto run_benchmark(std::vector<test_case_t> const& test_data, invocable_t&& func) -> double
{
    // warmup pass; primes the instruction cache and branch predictor so cold misses don't skew the results
    for (auto const& test_case : test_data) { do_not_optimize(func(test_case)); }

    auto aux = uint32_t{0};

    // timed pass
    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto const& test_case : test_data) do_not_optimize(func(test_case));

    _mm_lfence();
    auto const end_cycles = __rdtscp(&aux);
    _mm_lfence();

    auto const total_cycles = end_cycles - start_cycles;
    return static_cast<double>(total_cycles) / static_cast<double>(test_data.size());
}

auto main() -> int
{
    constexpr auto const sample_size = 10'000'000u;

    std::cout << "Generating " << sample_size << " random test cases...\n";
    auto const test_data = generate_test_data(sample_size);
    std::cout << "Data generated. Running benchmark...\n\n";

    auto pipeline = pipeline_t{};

    auto const cycles
        = run_benchmark(test_data, [&](test_case_t const& test_case) { return pipeline.run(test_case.in); });

    std::cout << std::fixed << std::setprecision(5);
    std::cout << "Pipeline : " << cycles << " cycles/iteration\n";

    return 0;
}

} // namespace
} // namespace crv

auto main() -> int
{
    return crv::main();
}
