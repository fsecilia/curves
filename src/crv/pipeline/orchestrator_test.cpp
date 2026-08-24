// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "orchestrator.hpp"
#include "report_timer.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::pipeline {
namespace {

struct orchestrator_test_t : Test
{
    using duration_t = fixed_t<uint64_t, 0>;
    using speed_t = fixed_t<int64_t, 52>;
    using gain_value_t = fixed_t<int64_t, 53>;
    using transformed_t = fixed_t<int128_t, 53>;
    using velocity_scale_t = fixed_t<uint64_t, 34>;
    using timestamp_t = uint64_t;

    struct mock_report_t
    {
        virtual ~mock_report_t() = default;
        MOCK_METHOD(bool, try_store, (int32_t, int32_t), (noexcept));
    };
    StrictMock<mock_report_t> mock_report;

    struct report_t
    {
        mock_report_t* mock = nullptr;
        int32_t original_x{};
        int32_t original_y{};
        bool is_valid = true;

        auto valid() const noexcept -> bool { return is_valid; }
        auto x() const noexcept -> int32_t { return original_x; }
        auto y() const noexcept -> int32_t { return original_y; }
        auto try_store(int32_t x, int32_t y) && noexcept -> bool { return mock->try_store(x, y); }
    };

    using timer_status_t = report_timer_t<duration_t>::status_t;
    using timer_result_t = report_timer_t<duration_t>::result_t;

    struct mock_timer_t
    {
        virtual ~mock_timer_t() = default;
        MOCK_METHOD(timer_result_t, call, (timestamp_t), (noexcept));
    };
    StrictMock<mock_timer_t> mock_timer;

    struct timer_t
    {
        using duration_t = orchestrator_test_t::duration_t;
        using timestamp_t = orchestrator_test_t::timestamp_t;
        using status_t = timer_status_t;
        using result_t = timer_result_t;

        mock_timer_t* mock = nullptr;
        auto operator()(timestamp_t timestamp) noexcept -> result_t { return mock->call(timestamp); }
    };

    struct velocity_result_t
    {
        speed_t value{};
        bool valid = false;

        constexpr auto operator==(velocity_result_t const&) const noexcept -> bool = default;
    };

    struct mock_velocity_t
    {
        virtual ~mock_velocity_t() = default;
        MOCK_METHOD(velocity_result_t, call, (int32_t, int32_t, duration_t, velocity_scale_t), (const, noexcept));
    };
    StrictMock<mock_velocity_t> mock_velocity;

    struct velocity_t
    {
        using duration_t = orchestrator_test_t::duration_t;
        using scale_t = velocity_scale_t;
        using out_t = speed_t;
        using result_t = velocity_result_t;

        mock_velocity_t* mock = nullptr;
        auto operator()(int32_t x, int32_t y, duration_t duration, scale_t scale) const noexcept -> result_t
        {
            return mock->call(x, y, duration, scale);
        }
    };

    struct mock_speed_filter_t
    {
        virtual ~mock_speed_filter_t() = default;
        MOCK_METHOD(speed_t, call, (speed_t, duration_t, duration_t), (noexcept));
    };
    StrictMock<mock_speed_filter_t> mock_speed_filter;

    struct speed_filter_t
    {
        mock_speed_filter_t* mock = nullptr;
        auto operator()(speed_t speed, duration_t half_life, duration_t duration) noexcept -> speed_t
        {
            return mock->call(speed, half_life, duration);
        }
    };

    struct prefetcher_t;

    struct mock_gain_t
    {
        virtual ~mock_gain_t() = default;
        MOCK_METHOD(void, prefetch, (int_t), (const, noexcept));
        MOCK_METHOD(gain_value_t, call, (speed_t, int_t), (const, noexcept));
    };
    StrictMock<mock_gain_t> mock_gain;

    struct gain_t
    {
        struct hint_t
        {
            int_t value{};
        };

        mock_gain_t* mock = nullptr;

        auto prefetch(hint_t const& hint, prefetcher_t const&) const noexcept -> void { mock->prefetch(hint.value); }

        auto evaluate(speed_t speed, hint_t& hint) const noexcept -> gain_value_t
        {
            return mock->call(speed, hint.value);
        }
    };

    struct output_transform_result_t
    {
        transformed_t x{};
        transformed_t y{};
        bool valid = false;
    };

    struct mock_output_transform_t
    {
        virtual ~mock_output_transform_t() = default;
        MOCK_METHOD(output_transform_result_t, call, (int32_t, int32_t, gain_value_t), (const, noexcept));
    };
    StrictMock<mock_output_transform_t> mock_output_transform;

    struct output_transform_t
    {
        using result_t = output_transform_result_t;

        mock_output_transform_t* mock = nullptr;

        auto operator()(int32_t x, int32_t y, gain_value_t gain) const noexcept -> result_t
        {
            return mock->call(x, y, gain);
        }
    };

    struct accumulator_reservation_t
    {
        int32_t x{};
        int32_t y{};
        bool valid = false;

        constexpr auto operator==(accumulator_reservation_t const&) const noexcept -> bool = default;
    };

    struct mock_accumulator_t
    {
        virtual ~mock_accumulator_t() = default;
        MOCK_METHOD(accumulator_reservation_t, reserve, (transformed_t, transformed_t), (const, noexcept));
        MOCK_METHOD(void, commit, (accumulator_reservation_t const&), (noexcept));
    };
    StrictMock<mock_accumulator_t> mock_accumulator;

    struct accumulator_t
    {
        using reservation_t = accumulator_reservation_t;

        mock_accumulator_t* mock = nullptr;

        auto reserve(transformed_t x, transformed_t y) const noexcept -> reservation_t { return mock->reserve(x, y); }
        auto commit(reservation_t const& reservation) noexcept -> void { mock->commit(reservation); }
    };

    struct mock_prefetcher_t
    {
        virtual ~mock_prefetcher_t() = default;
        MOCK_METHOD(void, prefetch, (void const*), (const, noexcept));
    };
    StrictMock<mock_prefetcher_t> mock_prefetcher;

    struct prefetcher_t
    {
        mock_prefetcher_t* mock = nullptr;
        auto prefetch(void const* address) const noexcept -> void { mock->prefetch(address); }
    };

    using sut_t
        = orchestrator_t<timer_t, velocity_t, speed_filter_t, gain_t, output_transform_t, accumulator_t, prefetcher_t>;

    static constexpr auto timestamp = timestamp_t{4'000'000};
    static constexpr auto duration = duration_t{250'000};
    static constexpr auto velocity_scale = velocity_scale_t{8000};
    static constexpr auto half_life = duration_t{1'500'000};
    static constexpr auto speed = speed_t{17};
    static constexpr auto filtered_speed = speed_t{13};
    static constexpr auto scalar_gain = gain_value_t{2};
    static constexpr auto transformed_x = transformed_t::literal(101);
    static constexpr auto transformed_y = transformed_t::literal(-202);
    static constexpr auto reservation = accumulator_t::reservation_t{.x = -7, .y = 9, .valid = true};

    sut_t::config_t config{
        .velocity_scale = velocity_scale,
        .half_life = half_life,
        .output_transform = output_transform_t{&mock_output_transform},
    };
    sut_t::state_t state{
        .timer = timer_t{&mock_timer},
        .speed_filter = speed_filter_t{&mock_speed_filter},
        .accumulator = accumulator_t{&mock_accumulator},
    };
    gain_t gain{&mock_gain};
    sut_t sut{
        .velocity = velocity_t{&mock_velocity},
        .prefetcher = prefetcher_t{&mock_prefetcher},
    };

    static_assert(sizeof(sut_t::state_t) == 64);

    auto report(int32_t x = 3, int32_t y = -4) noexcept -> report_t
    {
        return report_t{.mock = &mock_report, .original_x = x, .original_y = y};
    }

    auto process(report_t& input) noexcept -> pipeline_result_t
    {
        return sut.process(input, timestamp, config, state, gain);
    }

    auto expect_through_reservation(bool reservation_valid = true) -> void
    {
        auto const original_x = int32_t{3};
        auto const original_y = int32_t{-4};

        EXPECT_CALL(mock_timer, call(timestamp))
            .WillOnce(Return(timer_t::result_t{.duration = duration, .status = timer_t::status_t::ready}));
        EXPECT_CALL(mock_gain, prefetch(0));
        EXPECT_CALL(mock_velocity, call(original_x, original_y, duration, velocity_scale))
            .WillOnce(Return(velocity_t::result_t{.value = speed, .valid = true}));
        EXPECT_CALL(mock_speed_filter, call(speed, half_life, duration)).WillOnce(Return(filtered_speed));
        EXPECT_CALL(mock_gain, call(filtered_speed, 0)).WillOnce(Return(scalar_gain));
        EXPECT_CALL(mock_output_transform, call(original_x, original_y, scalar_gain))
            .WillOnce(Return(output_transform_t::result_t{.x = transformed_x, .y = transformed_y, .valid = true}));
        EXPECT_CALL(mock_accumulator, reserve(transformed_x, transformed_y))
            .WillOnce(Return(
                accumulator_t::reservation_t{.x = reservation.x, .y = reservation.y, .valid = reservation_valid}));
    }

    InSequence const seq{};
};

TEST_F(orchestrator_test_t, orchestrates_real_stage_order_and_commits_only_after_store)
{
    auto input = report();

    expect_through_reservation();
    EXPECT_CALL(mock_report, try_store(reservation.x, reservation.y)).WillOnce(Return(true));
    EXPECT_CALL(mock_accumulator, commit(reservation));

    EXPECT_EQ(process(input), pipeline_result_t::applied);
}

TEST_F(orchestrator_test_t, append_failure_keeps_observation_calls_but_does_not_commit_residual)
{
    auto input = report();

    expect_through_reservation();
    EXPECT_CALL(mock_report, try_store(reservation.x, reservation.y)).WillOnce(Return(false));

    EXPECT_EQ(process(input), pipeline_result_t::append_failed);
}

TEST_F(orchestrator_test_t, velocity_failure_stops_before_filter_and_preserves_report)
{
    auto const original_x = int32_t{3};
    auto const original_y = int32_t{-4};
    auto input = report(original_x, original_y);

    EXPECT_CALL(mock_timer, call(timestamp))
        .WillOnce(Return(timer_t::result_t{.duration = duration, .status = timer_t::status_t::ready}));
    EXPECT_CALL(mock_gain, prefetch(0));
    EXPECT_CALL(mock_velocity, call(original_x, original_y, duration, velocity_scale))
        .WillOnce(Return(velocity_t::result_t{}));

    EXPECT_EQ(process(input), pipeline_result_t::velocity_out_of_range);
}

TEST_F(orchestrator_test_t, transform_failure_keeps_observation_state_but_does_not_touch_emission_state)
{
    auto const original_x = int32_t{3};
    auto const original_y = int32_t{-4};
    auto input = report(original_x, original_y);

    EXPECT_CALL(mock_timer, call(timestamp))
        .WillOnce(Return(timer_t::result_t{.duration = duration, .status = timer_t::status_t::ready}));
    EXPECT_CALL(mock_gain, prefetch(0));
    EXPECT_CALL(mock_velocity, call(original_x, original_y, duration, velocity_scale))
        .WillOnce(Return(velocity_t::result_t{.value = speed, .valid = true}));
    EXPECT_CALL(mock_speed_filter, call(speed, half_life, duration)).WillOnce(Return(filtered_speed));
    EXPECT_CALL(mock_gain, call(filtered_speed, 0)).WillOnce(Return(scalar_gain));
    EXPECT_CALL(mock_output_transform, call(original_x, original_y, scalar_gain))
        .WillOnce(Return(output_transform_t::result_t{}));

    EXPECT_EQ(process(input), pipeline_result_t::transform_input_out_of_range);
}

TEST_F(orchestrator_test_t, invalid_reservation_does_not_mutate_report_or_commit_residual)
{
    auto input = report();

    expect_through_reservation(false);

    EXPECT_EQ(process(input), pipeline_result_t::output_out_of_range);
}

TEST_F(orchestrator_test_t, zero_half_life_bypasses_filter_without_changing_lookup_policy)
{
    config.half_life = duration_t{};
    auto input = report();

    EXPECT_CALL(mock_timer, call(timestamp))
        .WillOnce(Return(timer_t::result_t{.duration = duration, .status = timer_t::status_t::ready}));
    EXPECT_CALL(mock_gain, prefetch(0));
    EXPECT_CALL(mock_velocity, call(3, -4, duration, velocity_scale))
        .WillOnce(Return(velocity_t::result_t{.value = speed, .valid = true}));
    EXPECT_CALL(mock_gain, call(speed, 0)).WillOnce(Return(scalar_gain));
    EXPECT_CALL(mock_output_transform, call(3, -4, scalar_gain))
        .WillOnce(Return(output_transform_t::result_t{.x = transformed_x, .y = transformed_y, .valid = true}));
    EXPECT_CALL(mock_accumulator, reserve(transformed_x, transformed_y)).WillOnce(Return(reservation));
    EXPECT_CALL(mock_report, try_store(reservation.x, reservation.y)).WillOnce(Return(true));
    EXPECT_CALL(mock_accumulator, commit(reservation));

    EXPECT_EQ(process(input), pipeline_result_t::applied);
}

TEST_F(orchestrator_test_t, first_report_only_initializes_observation_timing)
{
    auto input = report();

    EXPECT_CALL(mock_timer, call(timestamp)).WillOnce(Return(timer_t::result_t{.status = timer_t::status_t::initial}));

    EXPECT_EQ(process(input), pipeline_result_t::warmup);
}

TEST_F(orchestrator_test_t, invalid_report_stops_before_observation_state)
{
    auto input = report();
    input.is_valid = false;

    EXPECT_EQ(process(input), pipeline_result_t::invalid_report);
}

TEST_F(orchestrator_test_t, invalid_timestamp_stops_before_spline_and_numerical_path)
{
    auto input = report();

    EXPECT_CALL(mock_timer, call(timestamp)).WillOnce(Return(timer_t::result_t{.status = timer_t::status_t::invalid}));

    EXPECT_EQ(process(input), pipeline_result_t::invalid_timestamp);
}

} // namespace
} // namespace crv::pipeline
