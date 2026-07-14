// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "filter.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/test/test.hpp>
#include <concepts>
#include <gmock/gmock.h>

namespace crv::pipeline::filters::one_euro {
namespace {

using x_t = fixed_t<int32_t, 16>;
using dx_t = fixed_t<int32_t, 16>;
using reciprocal_dt_ms_t = fixed_t<uint32_t, 16>;
using cutoff_step_t = fixed_t<uint64_t, 58>;
using alpha_t = fixed_t<uint64_t, 64>;
using dt_ns_t = uint64_t;
using dt_ns_fixed_t = fixed_t<uint64_t, 0>;

constexpr auto reciprocal_one = reciprocal_dt_ms_t::literal(uint32_t{1} << reciprocal_dt_ms_t::frac_bits);

//
// orchestration
//

struct pipeline_filters_one_euro_filter_test_t : Test
{
    struct mock_derivative_alpha_map_t
    {
        MOCK_METHOD(alpha_t, call, (cutoff_step_t));
    };
    StrictMock<mock_derivative_alpha_map_t> mock_derivative_alpha_map;

    struct derivative_alpha_map_t
    {
        mock_derivative_alpha_map_t* mock = nullptr;
        auto operator()(cutoff_step_t cutoff_step) noexcept -> alpha_t { return mock->call(cutoff_step); }
    };

    struct mock_derivative_estimator_t
    {
        MOCK_METHOD(dx_t, call, (x_t, reciprocal_dt_ms_t, alpha_t));
        MOCK_METHOD(dx_t, output, (), (const));
    };
    StrictMock<mock_derivative_estimator_t> mock_derivative_estimator;

    struct derivative_estimator_t
    {
        mock_derivative_estimator_t* mock = nullptr;

        template <is_fixed reciprocal_t>
        auto operator()(x_t x, reciprocal_t reciprocal_dt_ms, alpha_t alpha) noexcept -> dx_t
        {
            static_assert(std::same_as<reciprocal_t, reciprocal_dt_ms_t>);
            return mock->call(x, reciprocal_dt_ms, alpha);
        }

        auto output() const noexcept -> dx_t { return mock->output(); }
    };

    struct mock_cutoff_step_calculator_t
    {
        MOCK_METHOD(cutoff_step_t, call, (cutoff_step_t, cutoff_step_t, dx_t, dt_ns_fixed_t));
    };
    StrictMock<mock_cutoff_step_calculator_t> mock_cutoff_step_calculator;

    struct cutoff_step_calculator_t
    {
        mock_cutoff_step_calculator_t* mock = nullptr;

        template <is_fixed actual_dx_t, is_fixed actual_dt_t>
        auto operator()(cutoff_step_t omega_min, cutoff_step_t beta, actual_dx_t filtered_dx,
            actual_dt_t dt_ns) noexcept -> cutoff_step_t
        {
            static_assert(std::same_as<actual_dx_t, dx_t>);
            static_assert(std::same_as<actual_dt_t, dt_ns_fixed_t>);
            return mock->call(omega_min, beta, filtered_dx, dt_ns);
        }
    };

    struct mock_signal_alpha_map_t
    {
        MOCK_METHOD(alpha_t, call, (cutoff_step_t));
    };
    StrictMock<mock_signal_alpha_map_t> mock_signal_alpha_map;

    struct signal_alpha_map_t
    {
        mock_signal_alpha_map_t* mock = nullptr;
        auto operator()(cutoff_step_t cutoff_step) noexcept -> alpha_t { return mock->call(cutoff_step); }
    };

    struct mock_signal_ema_t
    {
        MOCK_METHOD(x_t, call, (x_t, alpha_t));
        MOCK_METHOD(x_t, output, (), (const));
    };
    StrictMock<mock_signal_ema_t> mock_signal_ema;

    struct signal_ema_t
    {
        mock_signal_ema_t* mock = nullptr;
        auto operator()(x_t x, alpha_t alpha) noexcept -> x_t { return mock->call(x, alpha); }
        auto output() const noexcept -> x_t { return mock->output(); }
    };

    using sut_t = filter_t<x_t, dx_t, cutoff_step_t, alpha_t, derivative_alpha_map_t, derivative_estimator_t,
        cutoff_step_calculator_t, signal_alpha_map_t, signal_ema_t>;

    auto make_sut(typename sut_t::params_t params) -> sut_t
    {
        return sut_t{
            params,
            derivative_alpha_map_t{&mock_derivative_alpha_map},
            derivative_estimator_t{&mock_derivative_estimator},
            cutoff_step_calculator_t{&mock_cutoff_step_calculator},
            signal_alpha_map_t{&mock_signal_alpha_map},
            signal_ema_t{&mock_signal_ema},
        };
    }
};

TEST_F(pipeline_filters_one_euro_filter_test_t, routes_one_sample_through_the_algorithm)
{
    constexpr auto params = sut_t::params_t{
        .omega_derivative = cutoff_step_t::literal(1),
        .omega_min = cutoff_step_t::literal(2),
        .beta = cutoff_step_t::literal(3),
    };

    constexpr auto input = x_t::literal(101);
    constexpr auto reciprocal = reciprocal_one;
    constexpr auto dt_ns = dt_ns_t{1'000'000};
    constexpr auto dt_fixed = dt_ns_fixed_t::literal(dt_ns);
    constexpr auto derivative_cutoff_step = cutoff_step_t::literal(1'000'000);
    constexpr auto derivative_alpha = alpha_t::literal(11);
    constexpr auto filtered_dx = dx_t::literal(22);
    constexpr auto signal_cutoff_step = cutoff_step_t::literal(33);
    constexpr auto signal_alpha = alpha_t::literal(44);
    constexpr auto expected_output = x_t::literal(55);

    {
        InSequence sequence;
        EXPECT_CALL(mock_derivative_alpha_map, call(derivative_cutoff_step)).WillOnce(Return(derivative_alpha));
        EXPECT_CALL(mock_derivative_estimator, call(input, reciprocal, derivative_alpha)).WillOnce(Return(filtered_dx));
        EXPECT_CALL(mock_cutoff_step_calculator, call(params.omega_min, params.beta, filtered_dx, dt_fixed))
            .WillOnce(Return(signal_cutoff_step));
        EXPECT_CALL(mock_signal_alpha_map, call(signal_cutoff_step)).WillOnce(Return(signal_alpha));
        EXPECT_CALL(mock_signal_ema, call(input, signal_alpha)).WillOnce(Return(expected_output));
    }

    auto sut = make_sut(params);
    EXPECT_EQ(sut(input, reciprocal, dt_ns), expected_output);
}

TEST_F(pipeline_filters_one_euro_filter_test_t, passes_derivative_cutoff_steps_above_signal_ceiling_unclamped)
{
    constexpr auto dt_ns = dt_ns_t{1'000'000};

    // exactly representable omega with scaled step is just above 31
    constexpr auto cutoff_ceiling = cutoff_step_t{31};
    constexpr auto omega_derivative_value = (cutoff_ceiling.value / dt_ns) + 1;
    constexpr auto omega_derivative = cutoff_step_t::literal(omega_derivative_value);
    constexpr auto derivative_cutoff_step = cutoff_step_t::literal(omega_derivative_value * dt_ns);
    static_assert(derivative_cutoff_step > cutoff_ceiling);

    constexpr auto params = sut_t::params_t{
        .omega_derivative = omega_derivative,
        .omega_min = cutoff_step_t::literal(2),
        .beta = cutoff_step_t::literal(3),
    };

    constexpr auto input = x_t::literal(101);
    constexpr auto derivative_alpha = alpha_t::literal(11);
    constexpr auto filtered_dx = dx_t::literal(22);
    constexpr auto signal_cutoff_step = cutoff_step_t::literal(23);
    constexpr auto signal_alpha = alpha_t::literal(24);
    constexpr auto expected_output = x_t::literal(25);

    EXPECT_CALL(mock_derivative_alpha_map, call(derivative_cutoff_step)).WillOnce(Return(derivative_alpha));
    EXPECT_CALL(mock_derivative_estimator, call(input, reciprocal_one, derivative_alpha)).WillOnce(Return(filtered_dx));
    EXPECT_CALL(
        mock_cutoff_step_calculator, call(params.omega_min, params.beta, filtered_dx, dt_ns_fixed_t::literal(dt_ns)))
        .WillOnce(Return(signal_cutoff_step));
    EXPECT_CALL(mock_signal_alpha_map, call(signal_cutoff_step)).WillOnce(Return(signal_alpha));
    EXPECT_CALL(mock_signal_ema, call(input, signal_alpha)).WillOnce(Return(expected_output));

    auto sut = make_sut(params);
    EXPECT_EQ(sut(input, reciprocal_one, dt_ns), expected_output);
}

TEST_F(pipeline_filters_one_euro_filter_test_t, routes_state_accessors_to_their_owners)
{
    constexpr auto expected_derivative = dx_t::literal(17);
    constexpr auto expected_signal = x_t::literal(23);

    EXPECT_CALL(mock_derivative_estimator, output()).WillOnce(Return(expected_derivative));
    EXPECT_CALL(mock_signal_ema, output()).WillOnce(Return(expected_signal));

    auto sut = make_sut(sut_t::params_t{});
    EXPECT_EQ(sut.derivative_state(), expected_derivative);
    EXPECT_EQ(sut.signal_state(), expected_signal);
}

//
// reciprocal/dt boundary contract
//

struct pipeline_filters_one_euro_filter_contract_test_t : Test
{
    struct alpha_map_t
    {
        constexpr auto operator()(cutoff_step_t) const noexcept -> alpha_t { return {}; }
    };

    struct derivative_estimator_t
    {
        template <is_fixed reciprocal_t> constexpr auto operator()(x_t, reciprocal_t, alpha_t) noexcept -> dx_t
        {
            return {};
        }

        constexpr auto output() const noexcept -> dx_t { return {}; }
    };

    struct cutoff_step_calculator_t
    {
        template <is_fixed actual_dx_t, is_fixed actual_dt_t>
        constexpr auto operator()(cutoff_step_t, cutoff_step_t, actual_dx_t, actual_dt_t) const noexcept
            -> cutoff_step_t
        {
            return {};
        }
    };

    struct signal_ema_t
    {
        constexpr auto operator()(x_t x, alpha_t) noexcept -> x_t { return x; }
        constexpr auto output() const noexcept -> x_t { return {}; }
    };

    using sut_t = filter_t<x_t, dx_t, cutoff_step_t, alpha_t, alpha_map_t, derivative_estimator_t,
        cutoff_step_calculator_t, alpha_map_t, signal_ema_t>;
    sut_t sut{sut_t::params_t{}};

    static constexpr auto dt_1khz = dt_ns_t{1'000'000};
    static constexpr auto zero_reciprocal_threshold
        = dt_ns_t{2'000'000} * (dt_ns_t{1} << reciprocal_dt_ms_t::frac_bits);

    auto reciprocal_for(dt_ns_t dt_ns) -> reciprocal_dt_ms_t
    {
        return to_fixed<reciprocal_dt_ms_t>(1e6 / static_cast<double>(dt_ns));
    }
};

TEST_F(pipeline_filters_one_euro_filter_contract_test_t, accepts_a_consistent_rounded_reciprocal)
{
    constexpr auto dt_ns = dt_ns_t{1'300'000};
    EXPECT_NO_FATAL_FAILURE(static_cast<void>(sut(x_t{10}, reciprocal_for(dt_ns), dt_ns)));
}

TEST_F(pipeline_filters_one_euro_filter_contract_test_t, accepts_zero_in_the_long_idle_rounding_regime)
{
    EXPECT_NO_FATAL_FAILURE(static_cast<void>(sut(x_t{10}, reciprocal_dt_ms_t{}, zero_reciprocal_threshold)));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(pipeline_filters_one_euro_filter_contract_test_t, rejects_zero_dt)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(sut(x_t{}, reciprocal_dt_ms_t{}, 0)), "zero dt_ns");
}

TEST_F(pipeline_filters_one_euro_filter_contract_test_t, rejects_an_inconsistent_reciprocal)
{
    constexpr auto bad_reciprocal = reciprocal_dt_ms_t::literal((uint32_t{1} << reciprocal_dt_ms_t::frac_bits) + 1);
    EXPECT_DEBUG_DEATH(static_cast<void>(sut(x_t{}, bad_reciprocal, dt_1khz)), "does not match dt_ns");
}

TEST_F(pipeline_filters_one_euro_filter_contract_test_t, rejects_zero_before_the_long_idle_tolerance)
{
    // implementation deliberately permits one product ulp of slack
    constexpr auto just_outside_tolerance = zero_reciprocal_threshold - 3;
    EXPECT_DEBUG_DEATH(
        static_cast<void>(sut(x_t{}, reciprocal_dt_ms_t{}, just_outside_tolerance)), "does not match dt_ns");
}

#endif

} // namespace
} // namespace crv::pipeline::filters::one_euro
