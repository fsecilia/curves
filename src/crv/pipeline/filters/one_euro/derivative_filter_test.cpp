// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "derivative_filter.hpp"
#include <crv/test/test.hpp>
#include <optional>

namespace crv::pipeline::filters::one_euro {
namespace {

using x_t = fixed_t<int64_t, 4>;
using dx_t = fixed_t<int64_t, 4>;
using cutoff_rate_t = fixed_t<int64_t, 4>;
using test_cutoff_interval_t = fixed_t<int64_t, 4>;
using dt_ns_t = fixed_t<uint64_t, 0>;

struct fake_cutoff_interval_calculator_t
{
    using cutoff_interval_t = test_cutoff_interval_t;

    std::optional<cutoff_interval_t> result;

    constexpr auto calc(cutoff_rate_t, dt_ns_t) const noexcept -> std::optional<cutoff_interval_t> { return result; }
};

using sut_t = derivative_filter_t<x_t, dx_t, cutoff_rate_t, fake_cutoff_interval_calculator_t>;

constexpr auto finite_interval = test_cutoff_interval_t{1};
constexpr auto derivative_cutoff_rate = cutoff_rate_t::literal(8); // 1/2

TEST(pipeline_filters_one_euro_derivative_filter_test, filters_positive_delta)
{
    auto sut = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{finite_interval}};

    // (0 + 1/2*(6 - 2))/(1 + 1) = 1
    EXPECT_EQ(dx_t{1}, sut(x_t{6}, x_t{2}, derivative_cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, filters_negative_delta)
{
    auto sut = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{finite_interval}};

    // (0 + 1/2*(2 - 6))/(1 + 1) = -1
    EXPECT_EQ(dx_t{-1}, sut(x_t{2}, x_t{6}, derivative_cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, decays_previous_derivative_when_delta_is_zero)
{
    auto sut = sut_t{dx_t{2}, fake_cutoff_interval_calculator_t{finite_interval}};

    // (2 + 1/2*(5 - 5))/(1 + 1) = 1
    EXPECT_EQ(dx_t{1}, sut(x_t{5}, x_t{5}, derivative_cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, carries_derivative_state_between_samples)
{
    auto sut = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{finite_interval}};

    EXPECT_EQ(dx_t{1}, sut(x_t{6}, x_t{2}, derivative_cutoff_rate, dt_ns_t{2}));

    // (1 + 1/2*(6 - 2))/(1 + 1) = 3/2
    EXPECT_EQ(dx_t::literal(24), sut(x_t{6}, x_t{2}, derivative_cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, uses_previous_filtered_signal_as_derivative_baseline)
{
    auto from_low_baseline = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{finite_interval}};
    auto from_high_baseline = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{finite_interval}};

    // The same current input produces different derivatives solely from the supplied previous filtered signal.
    EXPECT_EQ(dx_t{1}, from_low_baseline(x_t{7}, x_t{3}, derivative_cutoff_rate, dt_ns_t{2}));
    EXPECT_EQ(dx_t::literal(4), from_high_baseline(x_t{7}, x_t{6}, derivative_cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, uses_supplied_finite_interval_in_denominator)
{
    auto interval_one = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{test_cutoff_interval_t{1}}};
    auto interval_three = sut_t{dx_t{}, fake_cutoff_interval_calculator_t{test_cutoff_interval_t{3}}};

    EXPECT_EQ(dx_t{1}, interval_one(x_t{6}, x_t{2}, derivative_cutoff_rate, dt_ns_t{2}));
    EXPECT_EQ(dx_t::literal(8), interval_three(x_t{6}, x_t{2}, derivative_cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, empty_interval_uses_exact_raw_derivative_limit)
{
    auto sut = sut_t{dx_t{5}, fake_cutoff_interval_calculator_t{std::nullopt}};

    // The limiting path ignores previous derivative state: (8 - 2)/3 = 2.
    EXPECT_EQ(dx_t{2}, sut(x_t{8}, x_t{2}, derivative_cutoff_rate, dt_ns_t{3}));
}

TEST(pipeline_filters_one_euro_derivative_filter_test, reset_replaces_recursive_derivative_state)
{
    auto sut = sut_t{dx_t{2}, fake_cutoff_interval_calculator_t{finite_interval}};

    EXPECT_EQ(dx_t{1}, sut(x_t{5}, x_t{5}, derivative_cutoff_rate, dt_ns_t{2}));

    sut.reset(dx_t{4});

    EXPECT_EQ(dx_t{2}, sut(x_t{5}, x_t{5}, derivative_cutoff_rate, dt_ns_t{2}));
}

struct cutoff_interval_calculator_spy_state_t
{
    cutoff_rate_t cutoff_rate{};
    dt_ns_t dt_ns{};
    bool called{};
};

struct cutoff_interval_calculator_spy_t
{
    using cutoff_interval_t = test_cutoff_interval_t;

    cutoff_interval_calculator_spy_state_t* state;
    std::optional<cutoff_interval_t> result;

    constexpr auto calc(cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) const noexcept -> std::optional<cutoff_interval_t>
    {
        state->cutoff_rate = cutoff_rate;
        state->dt_ns = dt_ns;
        state->called = true;
        return result;
    }
};

TEST(pipeline_filters_one_euro_derivative_filter_test, forwards_cutoff_rate_and_elapsed_time_to_interval_calculator)
{
    auto state = cutoff_interval_calculator_spy_state_t{};
    using spy_sut_t = derivative_filter_t<x_t, dx_t, cutoff_rate_t, cutoff_interval_calculator_spy_t>;
    auto sut = spy_sut_t{dx_t{}, cutoff_interval_calculator_spy_t{&state, finite_interval}};

    constexpr auto cutoff_rate = cutoff_rate_t::literal(12);
    constexpr auto dt_ns = dt_ns_t{7};

    (void)sut(x_t{6}, x_t{2}, cutoff_rate, dt_ns);

    ASSERT_TRUE(state.called);
    EXPECT_EQ(cutoff_rate, state.cutoff_rate);
    EXPECT_EQ(dt_ns, state.dt_ns);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
