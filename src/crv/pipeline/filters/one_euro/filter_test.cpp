// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "filter.hpp"
#include "params.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <optional>

namespace crv::pipeline::filters::one_euro {
namespace {

using x_t = fixed_t<int64_t, 4>;
using dx_t = fixed_t<int64_t, 4>;
using cutoff_rate_t = fixed_t<int64_t, 4>;
using cutoff_slope_t = fixed_t<int64_t, 4>;
using dt_ns_t = fixed_t<uint64_t, 0>;
using params_t = one_euro::params_t<cutoff_rate_t, cutoff_slope_t>;

constexpr auto params = params_t{
    .derivative_cutoff_rate = cutoff_rate_t{2},
    .minimum_cutoff_rate = cutoff_rate_t{1},
    .cutoff_slope = cutoff_slope_t::literal(8),
};

struct first_sample_derivative_filter_t
{
    inline static auto reset_count = unsigned{};
    inline static auto call_count = unsigned{};

    static void clear() noexcept
    {
        reset_count = 0;
        call_count = 0;
    }

    void reset() noexcept { ++reset_count; }

    auto operator()(x_t, x_t, cutoff_rate_t, dt_ns_t) noexcept -> dx_t
    {
        ++call_count;
        return {};
    }
};

struct first_sample_signal_cutoff_rate_calculator_t
{
    inline static auto calc_count = unsigned{};

    static void clear() noexcept { calc_count = 0; }

    auto try_calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t, dx_t) const noexcept
        -> std::optional<cutoff_rate_t>
    {
        return minimum_cutoff_rate;
    }

    auto calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t, dx_t) const noexcept -> cutoff_rate_t
    {
        ++calc_count;
        return minimum_cutoff_rate;
    }
};

struct first_sample_signal_filter_t
{
    inline static auto output_count = unsigned{};
    inline static auto reset_count = unsigned{};
    inline static auto call_count = unsigned{};
    inline static auto reset_value = x_t{};

    static void clear() noexcept
    {
        output_count = 0;
        reset_count = 0;
        call_count = 0;
        reset_value = {};
    }

    auto output() const noexcept -> x_t
    {
        ++output_count;
        return reset_value;
    }

    void reset(x_t input) noexcept
    {
        ++reset_count;
        reset_value = input;
    }

    auto operator()(x_t input, cutoff_rate_t, dt_ns_t) noexcept -> x_t
    {
        ++call_count;
        return input;
    }
};

using first_sample_sut_t = filter_t<x_t, dx_t, params_t, first_sample_derivative_filter_t,
    first_sample_signal_cutoff_rate_calculator_t, first_sample_signal_filter_t>;

TEST(pipeline_filters_one_euro_filter_test, first_sample_seeds_recursive_state_and_is_returned_unchanged)
{
    first_sample_derivative_filter_t::clear();
    first_sample_signal_cutoff_rate_calculator_t::clear();
    first_sample_signal_filter_t::clear();

    auto sut = first_sample_sut_t{params};

    constexpr auto input = x_t{37};
    constexpr auto dt_ns = dt_ns_t{500'000'123};

    EXPECT_EQ(input, sut(input, dt_ns));

    EXPECT_EQ(1u, first_sample_derivative_filter_t::reset_count);
    EXPECT_EQ(1u, first_sample_signal_filter_t::reset_count);
    EXPECT_EQ(input, first_sample_signal_filter_t::reset_value);

    EXPECT_EQ(0u, first_sample_signal_filter_t::output_count);
    EXPECT_EQ(0u, first_sample_derivative_filter_t::call_count);
    EXPECT_EQ(0u, first_sample_signal_cutoff_rate_calculator_t::calc_count);
    EXPECT_EQ(0u, first_sample_signal_filter_t::call_count);

    constexpr auto next_input = x_t{41};
    constexpr auto next_dt_ns = dt_ns_t{7};

    EXPECT_EQ(next_input, sut(next_input, next_dt_ns));

    EXPECT_EQ(1u, first_sample_derivative_filter_t::reset_count);
    EXPECT_EQ(1u, first_sample_signal_filter_t::reset_count);
    EXPECT_EQ(1u, first_sample_signal_filter_t::output_count);
    EXPECT_EQ(1u, first_sample_derivative_filter_t::call_count);
    EXPECT_EQ(1u, first_sample_signal_cutoff_rate_calculator_t::calc_count);
    EXPECT_EQ(1u, first_sample_signal_filter_t::call_count);
}

struct mock_derivative_filter_t
{
    virtual ~mock_derivative_filter_t() = default;
    MOCK_METHOD(void, reset, (), (noexcept));
    MOCK_METHOD(dx_t, call, (x_t, x_t, cutoff_rate_t, dt_ns_t), (noexcept));
};

struct derivative_filter_delegate_t
{
    mock_derivative_filter_t* mock = nullptr;

    void reset() noexcept { mock->reset(); }
    auto operator()(
        x_t input, x_t previous_filtered_input, cutoff_rate_t derivative_cutoff_rate, dt_ns_t dt_ns) noexcept -> dx_t
    {
        return mock->call(input, previous_filtered_input, derivative_cutoff_rate, dt_ns);
    }

    auto operator==(derivative_filter_delegate_t const&) const noexcept -> bool = default;
};

struct mock_signal_cutoff_rate_calculator_t
{
    virtual ~mock_signal_cutoff_rate_calculator_t() = default;
    MOCK_METHOD(cutoff_rate_t, calc, (cutoff_rate_t, cutoff_slope_t, dx_t), (const, noexcept));
};

struct signal_cutoff_rate_calculator_delegate_t
{
    mock_signal_cutoff_rate_calculator_t* mock = nullptr;

    auto try_calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t, dx_t) const noexcept
        -> std::optional<cutoff_rate_t>
    {
        return minimum_cutoff_rate;
    }

    auto calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope, dx_t filtered_derivative) const noexcept
        -> cutoff_rate_t
    {
        return mock->calc(minimum_cutoff_rate, cutoff_slope, filtered_derivative);
    }

    auto operator==(signal_cutoff_rate_calculator_delegate_t const&) const noexcept -> bool = default;
};

struct mock_signal_filter_t
{
    virtual ~mock_signal_filter_t() = default;
    MOCK_METHOD(x_t, output, (), (const, noexcept));
    MOCK_METHOD(void, reset, (x_t), (noexcept));
    MOCK_METHOD(x_t, call, (x_t, cutoff_rate_t, dt_ns_t), (noexcept));
};

struct signal_filter_delegate_t
{
    mock_signal_filter_t* mock = nullptr;

    auto output() const noexcept -> x_t { return mock->output(); }
    void reset(x_t input) noexcept { mock->reset(input); }
    auto operator()(x_t input, cutoff_rate_t cutoff_rate, dt_ns_t dt_ns) noexcept -> x_t
    {
        return mock->call(input, cutoff_rate, dt_ns);
    }

    auto operator==(signal_filter_delegate_t const&) const noexcept -> bool = default;
};

using injected_sut_t = filter_t<x_t, dx_t, params_t, derivative_filter_delegate_t,
    signal_cutoff_rate_calculator_delegate_t, signal_filter_delegate_t>;

TEST(pipeline_filters_one_euro_filter_test, injected_state_is_initialized_and_orchestrates_one_sample)
{
    StrictMock<mock_derivative_filter_t> mock_derivative_filter;
    StrictMock<mock_signal_cutoff_rate_calculator_t> mock_signal_cutoff_rate_calculator;
    StrictMock<mock_signal_filter_t> mock_signal_filter;

    constexpr auto input = x_t{50};
    constexpr auto previous_filtered_input = x_t{37};
    constexpr auto filtered_derivative = dx_t{-3};
    constexpr auto cutoff_rate = cutoff_rate_t{5};
    constexpr auto output = x_t{42};

    // Deliberately far beyond an ordinary polling interval. The orchestrator forwards every positive elapsed interval
    // unchanged; interval limiting belongs to the leaf filters.
    constexpr auto dt_ns = dt_ns_t{500'000'123};

    {
        InSequence sequence;

        EXPECT_CALL(mock_signal_filter, output()).WillOnce(Return(previous_filtered_input));
        EXPECT_CALL(mock_derivative_filter, call(input, previous_filtered_input, params.derivative_cutoff_rate, dt_ns))
            .WillOnce(Return(filtered_derivative));
        EXPECT_CALL(mock_signal_cutoff_rate_calculator,
            calc(params.minimum_cutoff_rate, params.cutoff_slope, filtered_derivative))
            .WillOnce(Return(cutoff_rate));
        EXPECT_CALL(mock_signal_filter, call(input, cutoff_rate, dt_ns)).WillOnce(Return(output));
    }

    auto sut = injected_sut_t{params, derivative_filter_delegate_t{&mock_derivative_filter},
        signal_cutoff_rate_calculator_delegate_t{&mock_signal_cutoff_rate_calculator},
        signal_filter_delegate_t{&mock_signal_filter}};

    EXPECT_EQ(output, sut(input, dt_ns));
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
