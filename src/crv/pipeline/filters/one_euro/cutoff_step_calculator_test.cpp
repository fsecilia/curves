// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "cutoff_step_calculator.hpp"
#include <crv/math/int_traits.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::pipeline::filters::one_euro {
namespace {

struct pipeline_filters_one_euro_cutoff_step_calculator_test_t : Test
{
    using dx_t = fixed_t<int32_t, 16>;
    using dx_magnitude_t = fixed_t<make_unsigned_t<dx_t::value_t>, dx_t::frac_bits>;
    using dt_ns_t = fixed_t<uint64_t, 0>;
    using cutoff_step_t = fixed_t<uint64_t, 58>;
    using cutoff_rate_t = cutoff_step_t;

    using adaptive_cutoff_rate_t = fixed::product_t<cutoff_rate_t, dx_magnitude_t>;
    using unclamped_cutoff_step_t = fixed::product_t<cutoff_rate_t, dt_ns_t>;

    struct mock_cutoff_rate_combiner_t
    {
        virtual ~mock_cutoff_rate_combiner_t() = default;
        MOCK_METHOD(
            cutoff_rate_t, call, (cutoff_rate_t omega_min, adaptive_cutoff_rate_t adaptive_cutoff_rate), (const));
    };
    StrictMock<mock_cutoff_rate_combiner_t> mock_cutoff_rate_combiner;

    struct cutoff_rate_combiner_t
    {
        mock_cutoff_rate_combiner_t* mock = nullptr;
        auto operator()(cutoff_rate_t omega_min, adaptive_cutoff_rate_t adaptive_cutoff_rate) const noexcept
            -> cutoff_rate_t
        {
            return mock->call(omega_min, adaptive_cutoff_rate);
        }
    };

    struct mock_cutoff_step_clamp_t
    {
        virtual ~mock_cutoff_step_clamp_t() = default;
        MOCK_METHOD(cutoff_step_t, call, (unclamped_cutoff_step_t cutoff_step), (const));
    };
    StrictMock<mock_cutoff_step_clamp_t> mock_cutoff_step_clamp;

    struct cutoff_step_clamp_t
    {
        mock_cutoff_step_clamp_t* mock = nullptr;
        auto operator()(unclamped_cutoff_step_t cutoff_step) const noexcept -> cutoff_step_t
        {
            return mock->call(cutoff_step);
        }
    };

    using sut_t = cutoff_step_calculator_t<cutoff_step_t, cutoff_rate_combiner_t, cutoff_step_clamp_t>;
    sut_t sut{
        cutoff_rate_combiner_t{&mock_cutoff_rate_combiner},
        cutoff_step_clamp_t{&mock_cutoff_step_clamp},
    };
};

TEST_F(pipeline_filters_one_euro_cutoff_step_calculator_test_t,
    calculates_adaptive_rate_scales_combined_rate_and_returns_clamped_step)
{
    // adaptive rate = beta * abs(filtered_dx) = 3 * abs(-4) = 12
    constexpr auto omega_min = cutoff_rate_t{2};
    constexpr auto beta = cutoff_rate_t{3};
    constexpr auto filtered_dx = dx_t{-4};
    constexpr auto expected_adaptive_cutoff_rate = adaptive_cutoff_rate_t{12};

    // deliberately return something other than omega_min + adaptive rate; proves calculator uses combiner's result
    constexpr auto combined_cutoff_rate = cutoff_rate_t{17};

    // unclamped step = combined rate * dt = 17 * 5 = 85
    constexpr auto dt_ns = dt_ns_t{5};
    constexpr auto expected_unclamped_cutoff_step = unclamped_cutoff_step_t{85};

    // deliberately unrelated to the unclamped value so return routing is tested
    constexpr auto clamped_cutoff_step = cutoff_step_t{23};

    EXPECT_CALL(mock_cutoff_rate_combiner, call(omega_min, expected_adaptive_cutoff_rate))
        .WillOnce(Return(combined_cutoff_rate));
    EXPECT_CALL(mock_cutoff_step_clamp, call(expected_unclamped_cutoff_step)).WillOnce(Return(clamped_cutoff_step));

    EXPECT_EQ(sut(omega_min, beta, filtered_dx, dt_ns), clamped_cutoff_step);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
