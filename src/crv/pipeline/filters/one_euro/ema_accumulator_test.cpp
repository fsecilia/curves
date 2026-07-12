// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "ema_accumulator.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/test/test.hpp>

namespace crv::pipeline::filters::one_euro {
namespace {

using sample_t = fixed_t<int32_t, 16>;
using smoothing_factor_t = fixed_t<uint32_t, 32>;
using sut_t = ema_accumulator_t<sample_t>;

constexpr auto sample_zero = sample_t{};
constexpr auto sample_one = sample_t{1};
constexpr auto alpha_half = smoothing_factor_t::literal(uint32_t{1} << 31);
constexpr auto alpha_max = smoothing_factor_t::literal(max<uint32_t>());

// signal must be signed
template <typename t>
concept is_ema_instantiable = requires { typename ema_accumulator_t<t>; };
static_assert(!is_ema_instantiable<fixed_t<uint32_t, 16>>);

constexpr auto zero_alpha_holds_state() noexcept -> bool
{
    auto sut = sut_t{sample_t{10}};
    return sut(sample_t{100}, smoothing_factor_t{}) == sample_t{10};
}
static_assert(zero_alpha_holds_state());

constexpr auto steady_state_is_identity() noexcept -> bool
{
    auto const sample = sample_t{37};
    auto sut = sut_t{sample};
    return sut(sample, alpha_half) == sample;
}
static_assert(steady_state_is_identity());

constexpr auto step_response_at_half_alpha() noexcept -> bool
{
    auto sut = sut_t{sample_zero};
    if (sut(sample_one, alpha_half) != sample_t::literal(1 << 15)) return false;
    if (sut(sample_one, alpha_half) != sample_t::literal(3 << 14)) return false;
    return sut(sample_one, alpha_half) == sample_t::literal(7 << 13);
}
static_assert(step_response_at_half_alpha());

constexpr auto negative_step_response() noexcept -> bool
{
    auto sut = sut_t{sample_zero};
    return sut(sample_t{-1}, alpha_half) == sample_t{-1} >> 1;
}
static_assert(negative_step_response());

constexpr auto state_matches_returned_output() noexcept -> bool
{
    auto sut = sut_t{};
    auto const output = sut(sample_one, alpha_half);
    return sut.output() == output;
}
static_assert(state_matches_returned_output());

constexpr auto negative_full_scale_error_saturates() noexcept -> bool
{
    auto sut = sut_t{max<sample_t>()};
    auto const expected = max<sample_t>() + min<sample_t>() / 2;
    return sut(min<sample_t>(), alpha_half) == expected;
}
static_assert(negative_full_scale_error_saturates());

constexpr auto positive_full_scale_error_saturates() noexcept -> bool
{
    auto sut = sut_t{min<sample_t>()};
    auto const expected = min<sample_t>() + max<sample_t>() / 2 + sample_t::literal(1);
    return sut(max<sample_t>(), alpha_half) == expected;
}
static_assert(positive_full_scale_error_saturates());

// policy discriminator; correction must narrow using rne instead of the default truncation
constexpr auto correction_uses_rne() noexcept -> bool
{
    using truncating_sut_t = ema_accumulator_t<sample_t, shifter_t<rounding_modes::shr::truncate>{}>;

    auto truncating_sut = truncating_sut_t{};
    auto rne_sut = sut_t{};
    auto const input = sample_t::literal(3);

    return truncating_sut(input, alpha_half).value == 1 && rne_sut(input, alpha_half).value == 2;
}
static_assert(correction_uses_rne());

constexpr auto maximum_alpha_is_effective_passthrough() noexcept -> bool
{
    auto sut = sut_t{};
    auto const input = sample_t{1000};
    return sut(input, alpha_max) == input;
}
static_assert(maximum_alpha_is_effective_passthrough());

// full-scale jump saturates signed error before alpha is applied, so even maximum alpha cannot go to max in one step
constexpr auto saturated_error_limits_maximum_alpha_step() noexcept -> bool
{
    auto sut = sut_t{min<sample_t>()};
    return sut(max<sample_t>(), alpha_max) == sample_t::literal(-1);
}
static_assert(saturated_error_limits_maximum_alpha_step());

TEST(ema_accumulator_test, converges_to_half_alpha_stall_residual)
{
    auto const sample = sample_t{1000};
    auto sut = sut_t{};
    for (auto i = 0; i < 200; ++i) sut(sample, alpha_half);
    EXPECT_EQ(sut.output().value, sample.value - 1);
}

TEST(ema_accumulator_test, does_not_drift_at_steady_state)
{
    auto const sample = sample_t{12345};
    auto sut = sut_t{sample};
    for (auto i = 0; i < 10'000; ++i) sut(sample, alpha_half);
    EXPECT_EQ(sut.output(), sample);
}

TEST(ema_accumulator_test, small_alpha_stall_is_symmetric)
{
    auto const sample = sample_t{500};
    auto const alpha = smoothing_factor_t::literal(uint32_t{1} << 24); // 1/256

    auto below = sut_t{};
    auto above = sut_t{sample_t{1000}};
    for (auto i = 0; i < 10'000; ++i)
    {
        below(sample, alpha);
        above(sample, alpha);
    }

    EXPECT_EQ(below.output().value, sample.value - 128);
    EXPECT_EQ(above.output().value, sample.value + 128);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
