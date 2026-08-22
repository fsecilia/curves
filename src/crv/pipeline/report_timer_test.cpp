// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "report_timer.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline {
namespace {

struct report_timer_test_t : Test
{
    using duration_t = fixed_t<uint64_t, 0>;
    using sut_t = report_timer_t<duration_t>;

    sut_t sut{};
};

struct initialized_report_timer_test_t : report_timer_test_t
{
    initialized_report_timer_test_t() { (void)sut(1'000'000); }
};

struct advanced_report_timer_test_t : initialized_report_timer_test_t
{
    advanced_report_timer_test_t() { (void)sut(1'250'000); }
};

TEST_F(report_timer_test_t, first_observation_initializes_without_duration)
{
    EXPECT_EQ(sut(1'000'000), (sut_t::result_t{.status = sut_t::status_t::initial}));
    EXPECT_TRUE(sut.initialized());
    EXPECT_EQ(sut.previous_timestamp(), 1'000'000u);
}

TEST_F(initialized_report_timer_test_t, next_observation_returns_elapsed_duration)
{
    EXPECT_EQ(sut(1'250'000), (sut_t::result_t{.duration = duration_t{250'000}, .status = sut_t::status_t::ready}));
}

TEST_F(initialized_report_timer_test_t, equal_timestamp_rebases_to_observed_value)
{
    EXPECT_EQ(sut(1'000'000).status, sut_t::status_t::invalid);
    EXPECT_EQ(sut.previous_timestamp(), 1'000'000u);
    EXPECT_EQ(sut(1'250'000).duration, duration_t{250'000});
}

TEST_F(advanced_report_timer_test_t, backward_timestamp_rejects_once_and_rebases)
{
    EXPECT_EQ(sut(900'000).status, sut_t::status_t::invalid);
    EXPECT_EQ(sut.previous_timestamp(), 900'000u);

    EXPECT_EQ(sut(1'000'000), (sut_t::result_t{.duration = duration_t{100'000}, .status = sut_t::status_t::ready}));
}

TEST_F(initialized_report_timer_test_t, long_gap_remains_elapsed_duration)
{
    EXPECT_EQ(
        sut(3'001'000'000), (sut_t::result_t{.duration = duration_t{3'000'000'000}, .status = sut_t::status_t::ready}));
}

} // namespace
} // namespace crv::pipeline
