// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "signal_filter.hpp"
#include <crv/test/test.hpp>
#include <optional>

namespace crv::pipeline::filters::one_euro {
namespace {

using x_t = fixed_t<int64_t, 4>;
using cutoff_rate_t = fixed_t<int64_t, 4>;
using test_cutoff_interval_t = fixed_t<int64_t, 4>;
using dt_ns_t = fixed_t<uint64_t, 0>;

struct fake_cutoff_interval_calculator_t
{
    using cutoff_interval_t = test_cutoff_interval_t;

    std::optional<cutoff_interval_t> result;

    constexpr auto calc(cutoff_rate_t, dt_ns_t) const noexcept -> std::optional<cutoff_interval_t> { return result; }
};

using sut_t = signal_filter_t<x_t, cutoff_rate_t, fake_cutoff_interval_calculator_t>;

constexpr auto finite_interval = test_cutoff_interval_t{1};
constexpr auto cutoff_rate = cutoff_rate_t::literal(8); // 1/2; value is opaque to the fake calculator.

TEST(pipeline_filters_one_euro_signal_filter_test, filters_input_above_previous_signal)
{
    auto sut = sut_t{x_t{2}, fake_cutoff_interval_calculator_t{finite_interval}};

    // (2 + 1*6)/(1 + 1) = 4
    EXPECT_EQ(x_t{4}, sut(x_t{6}, cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_signal_filter_test, filters_input_below_previous_signal)
{
    auto sut = sut_t{x_t{6}, fake_cutoff_interval_calculator_t{finite_interval}};

    // (6 + 1*2)/(1 + 1) = 4
    EXPECT_EQ(x_t{4}, sut(x_t{2}, cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_signal_filter_test, equal_input_preserves_signal_state)
{
    auto sut = sut_t{x_t{5}, fake_cutoff_interval_calculator_t{finite_interval}};

    EXPECT_EQ(x_t{5}, sut(x_t{5}, cutoff_rate, dt_ns_t{2}));
    EXPECT_EQ(x_t{5}, sut.output());
}

TEST(pipeline_filters_one_euro_signal_filter_test, carries_signal_state_between_samples)
{
    auto sut = sut_t{x_t{}, fake_cutoff_interval_calculator_t{finite_interval}};

    EXPECT_EQ(x_t{3}, sut(x_t{6}, cutoff_rate, dt_ns_t{2}));

    // (3 + 1*6)/(1 + 1) = 9/2
    EXPECT_EQ(x_t::literal(72), sut(x_t{6}, cutoff_rate, dt_ns_t{2}));
    EXPECT_EQ(x_t::literal(72), sut.output());
}

TEST(pipeline_filters_one_euro_signal_filter_test, uses_supplied_finite_interval_in_recurrence)
{
    auto interval_one = sut_t{x_t{}, fake_cutoff_interval_calculator_t{test_cutoff_interval_t{1}}};
    auto interval_three = sut_t{x_t{}, fake_cutoff_interval_calculator_t{test_cutoff_interval_t{3}}};

    EXPECT_EQ(x_t{4}, interval_one(x_t{8}, cutoff_rate, dt_ns_t{2}));
    EXPECT_EQ(x_t{6}, interval_three(x_t{8}, cutoff_rate, dt_ns_t{2}));
}

TEST(pipeline_filters_one_euro_signal_filter_test, empty_interval_uses_exact_input_limit)
{
    auto sut = sut_t{x_t{3}, fake_cutoff_interval_calculator_t{std::nullopt}};

    EXPECT_EQ(x_t{11}, sut(x_t{11}, cutoff_rate, dt_ns_t{7}));
    EXPECT_EQ(x_t{11}, sut.output());
}

TEST(pipeline_filters_one_euro_signal_filter_test, constructor_seeds_output)
{
    auto sut = sut_t{x_t{7}, fake_cutoff_interval_calculator_t{finite_interval}};

    EXPECT_EQ(x_t{7}, sut.output());
}

TEST(pipeline_filters_one_euro_signal_filter_test, reset_replaces_recursive_signal_state)
{
    auto sut = sut_t{x_t{2}, fake_cutoff_interval_calculator_t{finite_interval}};

    EXPECT_EQ(x_t{4}, sut(x_t{6}, cutoff_rate, dt_ns_t{2}));

    sut.reset(x_t{10});

    EXPECT_EQ(x_t{8}, sut(x_t{6}, cutoff_rate, dt_ns_t{2}));
    EXPECT_EQ(x_t{8}, sut.output());
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

TEST(pipeline_filters_one_euro_signal_filter_test, forwards_cutoff_rate_and_elapsed_time_to_interval_calculator)
{
    auto state = cutoff_interval_calculator_spy_state_t{};
    using spy_sut_t = signal_filter_t<x_t, cutoff_rate_t, cutoff_interval_calculator_spy_t>;
    auto sut = spy_sut_t{x_t{}, cutoff_interval_calculator_spy_t{&state, finite_interval}};

    constexpr auto forwarded_cutoff_rate = cutoff_rate_t::literal(12);
    constexpr auto dt_ns = dt_ns_t{7};

    (void)sut(x_t{6}, forwarded_cutoff_rate, dt_ns);

    ASSERT_TRUE(state.called);
    EXPECT_EQ(forwarded_cutoff_rate, state.cutoff_rate);
    EXPECT_EQ(dt_ns, state.dt_ns);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
