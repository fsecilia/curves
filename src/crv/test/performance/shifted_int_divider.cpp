// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/division/divider.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/test/performance/performance.hpp>
#include <climits>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace crv {
namespace {

using wide_t   = uint128_t;
using narrow_t = uint64_t;

constexpr auto narrow_width = sizeof(narrow_t) * CHAR_BIT;

struct test_case_t
{
    wide_t   dividend;
    narrow_t divisor;
};

/// pre-generates randomized inputs to keep generation latency out of the benchmark loop
auto generate_test_data(size_t sample_size) -> std::vector<test_case_t>
{
    auto data = std::vector<test_case_t>{};
    data.reserve(sample_size);

    auto rng = std::mt19937_64(std::random_device{}());

    // half width for dividend; the result is combined from 2 words
    auto dist_dividend = std::uniform_int_distribution<narrow_t>{0, max<narrow_t>()};

    // ensure divisor is strictly greater than 0 to avoid hardware traps during random generation
    auto dist_divisor = std::uniform_int_distribution<narrow_t>{1, max<narrow_t>()};

    for (size_t index = 0; index < sample_size; ++index)
    {
        auto const dividend = (static_cast<wide_t>(dist_dividend(rng)) << narrow_width) | dist_dividend(rng);

        auto divisor = dist_divisor(rng);

        // constrain divisors so they never take the slow path
        // while ((dividend >> narrow_width) > divisor) divisor = dist_divisor(rng);

        data.push_back({dividend, divisor});
    }

    return data;
}

/// executes the microbenchmark on a generic callable
template <typename invocable_t>
auto run_benchmark(std::vector<test_case_t> const& test_data, invocable_t&& division_func) -> double
{
    // warmup pass; primes the instruction cache and branch predictor so cold misses don't skew the results
    for (auto const& pair : test_data) { do_not_optimize(division_func(pair.dividend, pair.divisor)); }

    auto aux = uint32_t{0};

    // timed pass
    _mm_lfence();
    auto const start_cycles = __rdtsc();
    _mm_lfence();

    for (auto const& pair : test_data) do_not_optimize(division_func(pair.dividend, pair.divisor));

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

    // test native compiler division as a baseline
    auto const compiler_cycles
        = run_benchmark(test_data, [](wide_t dividend, narrow_t divisor) { return dividend / divisor; });

    // test wide_divider_t
    division::wide_divider_t<narrow_t, division::hardware_divider_t<narrow_t>> const divider{};
    auto const hardware_cycles = run_benchmark(test_data, [&divider](wide_t dividend, narrow_t divisor) {
        return divider(dividend, divisor, rounding_modes::div::truncate);
    });

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Compiler Baseline : " << compiler_cycles << " cycles/div\n";
    std::cout << "Inline Asm (x64)  : " << hardware_cycles << " cycles/div\n";

    return 0;
}

} // namespace
} // namespace crv

auto main() -> int
{
    return crv::main();
}
