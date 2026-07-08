// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "ema_section.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using sample_t = fixed_t<int32_t, 16>;
using smoothing_factor_t = fixed_t<uint32_t, 32>;
using sut_t = ema_section_t<sample_t>;

constexpr auto sample_zero = sample_t{0};
constexpr auto sample_one = sample_t{1};
constexpr auto alpha_half = smoothing_factor_t::literal(uint32_t{1} << 31); // 0.5
constexpr auto x_min = min<sample_t>();
constexpr auto x_max = max<sample_t>();

//
// static tests
//

constexpr auto zero_alpha_returns_initial_state() noexcept -> bool
{
    auto sut = sut_t{sample_t{10}};
    return sut(sample_t{100}, smoothing_factor_t{}) == sample_t{10};
}
static_assert(zero_alpha_returns_initial_state());

constexpr auto steady_state_is_identity() noexcept -> bool
{
    auto const sample = sample_t{37};
    auto sut = sut_t{sample};
    return sut(sample, alpha_half) == sample;
}
static_assert(steady_state_is_identity());

// a = 0.5, step 0 -> 1.0: 0.5, 0.75, 0.875
constexpr auto step_response_half_alpha() noexcept -> bool
{
    auto sut = sut_t{sample_zero};
    if (sut(sample_one, alpha_half) != sample_t::literal(1 << 15)) return false; // 0.5
    if (sut(sample_one, alpha_half) != sample_t::literal(3 << 14)) return false; // 0.75
    if (sut(sample_one, alpha_half) != sample_t::literal(7 << 13)) return false; // 0.875
    return true;
}
static_assert(step_response_half_alpha());

constexpr auto negative_step_response() noexcept -> bool
{
    auto sut = sut_t{sample_zero};
    return sut(sample_t{-1}, alpha_half) == sample_t{-1} >> 1; // -0.5
}
static_assert(negative_step_response());

constexpr auto state_matches_output() noexcept -> bool
{
    auto sut = sut_t{sample_zero};
    auto const y = sut(sample_one, alpha_half);
    return sut.output() == y;
}
static_assert(state_matches_output());

// full-scale negative swing saturates instead of wrapping
constexpr auto diff_saturates_negative() noexcept -> bool
{
    auto const x_low = x_min;
    auto sut = sut_t{x_max};

    // (sample - state) would be (min - max) which overflows. This must saturate.
    auto const expected = x_max + x_min / 2;

    return sut(x_low, alpha_half) == expected;
}
static_assert(diff_saturates_negative());

// full-scale positive swing saturates instead of wrapping
constexpr auto diff_saturates_positive() noexcept -> bool
{
    auto const x_high = x_max;
    auto sut = sut_t{x_min};

    // (sample - state) would be (max - min), which overflows. This must saturate.
    auto const expected = x_min + x_max / 2 + sample_t::literal(1);

    return sut(x_high, alpha_half) == expected;
}
static_assert(diff_saturates_positive());

// uses rne to narrow after multiply
constexpr auto multiply_narrows_using_rne() noexcept -> bool
{
    using sut_truncate_t = ema_section_t<sample_t, shifter_t<rounding_modes::shr::truncate>{}>;
    auto sut_truncate = sut_truncate_t{};
    auto sut_rne = sut_t{};

    auto const sample_three = sample_t::literal(3);
    auto const truncate_result = sut_truncate(sample_three, alpha_half);
    auto const rne_result = sut_rne(sample_three, alpha_half);

    return truncate_result == sample_t::literal(1) && rne_result == sample_t::literal(2);
}
static_assert(multiply_narrows_using_rne());

//
// runtime tests
//

TEST(ema_test, converges_to_stall_residual)
{
    // a = 1/2 -> stall at 1/(2a) = 1 ulp
    auto const sample = sample_t{1000};
    auto sut = sut_t{sample_zero};
    for (auto i = 0; i < 200; ++i) sut(sample, alpha_half);
    EXPECT_EQ(sut.output().value, sample.value - 1);
}

TEST(ema_test, no_drift_at_steady_state)
{
    auto const sample = sample_t{12345};
    auto sut = sut_t{sample};
    for (auto i = 0; i < 10'000; ++i) sut(sample, alpha_half);
    EXPECT_EQ(sut.output(), sample);
}

TEST(ema_test, small_alpha_stall_from_below)
{
    // a = 1/256 -> stall at 1/(2a) = 128 ulps
    auto const sample = sample_t{500};
    auto const alpha = smoothing_factor_t::literal(uint32_t{1} << 24);
    auto sut = sut_t{sample_zero};
    for (auto i = 0; i < 10'000; ++i) sut(sample, alpha);
    EXPECT_EQ(sut.output().value, sample.value - 128);
}

TEST(ema_test, small_alpha_stall_from_above_is_symmetric)
{
    // a = 1/256 -> stall at 1/(2a) = 128 ulps
    auto const sample = sample_t{500};
    auto const alpha = smoothing_factor_t::literal(uint32_t{1} << 24);
    auto sut = sut_t{sample_t{1000}};
    for (auto i = 0; i < 10'000; ++i) sut(sample, alpha);
    EXPECT_EQ(sut.output().value, sample.value + 128);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
