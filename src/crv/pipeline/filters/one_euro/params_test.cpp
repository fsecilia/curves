// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "params.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <optional>

namespace crv::pipeline::filters::one_euro {
namespace {

using cutoff_rate_t = fixed_t<int8_t, 4>;
using cutoff_slope_t = fixed_t<int8_t, 4>;
using dx_t = fixed_t<int8_t, 4>;
using sut_t = params_t<cutoff_rate_t, cutoff_slope_t>;

constexpr auto valid_params = sut_t{
    .derivative_cutoff_rate = cutoff_rate_t::literal(1),
    .minimum_cutoff_rate = cutoff_rate_t::literal(1),
    .cutoff_slope = cutoff_slope_t::literal(1),
};

static_assert(valid_params.validate<dx_t>());

constexpr auto zero_slope_params = [] {
    auto result = valid_params;
    result.cutoff_slope = cutoff_slope_t{};
    return result;
}();
static_assert(zero_slope_params.validate<dx_t>());

TEST(pipeline_filters_one_euro_params_test, derivative_cutoff_rate_must_be_positive)
{
    for (auto const invalid : {cutoff_rate_t::literal(-1), cutoff_rate_t{}})
    {
        auto params = valid_params;
        params.derivative_cutoff_rate = invalid;

        auto const result = params.validate<dx_t>();

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(sut_t::validation_error::derivative_cutoff_rate_not_positive, result.error());
    }
}

TEST(pipeline_filters_one_euro_params_test, minimum_cutoff_rate_must_be_positive)
{
    for (auto const invalid : {cutoff_rate_t::literal(-1), cutoff_rate_t{}})
    {
        auto params = valid_params;
        params.minimum_cutoff_rate = invalid;

        auto const result = params.validate<dx_t>();

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(sut_t::validation_error::minimum_cutoff_rate_not_positive, result.error());
    }
}

TEST(pipeline_filters_one_euro_params_test, cutoff_slope_must_not_be_negative)
{
    auto params = valid_params;
    params.cutoff_slope = cutoff_slope_t::literal(-1);

    auto const result = params.validate<dx_t>();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(sut_t::validation_error::cutoff_slope_negative, result.error());
}

struct test_signal_cutoff_rate_calculator_state_t
{
    cutoff_rate_t minimum_cutoff_rate{};
    cutoff_slope_t cutoff_slope{};
    dx_t filtered_derivative{};
    bool called{};
};

struct test_signal_cutoff_rate_calculator_t
{
    test_signal_cutoff_rate_calculator_state_t* state;
    std::optional<cutoff_rate_t> result;

    constexpr auto try_calc(cutoff_rate_t minimum_cutoff_rate, cutoff_slope_t cutoff_slope,
        dx_t filtered_derivative) const noexcept -> std::optional<cutoff_rate_t>
    {
        state->minimum_cutoff_rate = minimum_cutoff_rate;
        state->cutoff_slope = cutoff_slope;
        state->filtered_derivative = filtered_derivative;
        state->called = true;
        return result;
    }
};

TEST(pipeline_filters_one_euro_params_test, validates_signal_cutoff_rate_over_full_derivative_range)
{
    auto state = test_signal_cutoff_rate_calculator_state_t{};
    auto const calculator = test_signal_cutoff_rate_calculator_t{
        .state = &state,
        .result = std::nullopt,
    };

    auto const result = valid_params.validate<dx_t>(calculator);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(sut_t::validation_error::signal_cutoff_rate_overflow, result.error());

    ASSERT_TRUE(state.called);
    EXPECT_EQ(valid_params.minimum_cutoff_rate, state.minimum_cutoff_rate);
    EXPECT_EQ(valid_params.cutoff_slope, state.cutoff_slope);
    EXPECT_EQ(min<dx_t>(), state.filtered_derivative);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
