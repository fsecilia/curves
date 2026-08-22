// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual_accumulator.hpp"
#include <crv/test/test.hpp>
#include <array>

namespace crv::pipeline {
namespace {

struct residual_accumulator_test_t : Test
{
    using input_t = fixed_t<int128_t, 45>;
    using sut_t = residual_accumulator_t<input_t>;
    using residual_t = sut_t::residual_t;

    static constexpr auto half_raw = int128_t{1} << (input_t::frac_bits - 1);
    static constexpr auto quarter_raw = int128_t{1} << (input_t::frac_bits - 2);

    static_assert(sizeof(sut_t) == sizeof(residual_t) * 2);

    sut_t sut{};
};

TEST_F(residual_accumulator_test_t, reservation_does_not_change_state)
{
    auto const reservation = sut.reserve(input_t::literal(quarter_raw), input_t{});

    ASSERT_TRUE(reservation.valid);
    EXPECT_EQ(reservation.x, 0);
    EXPECT_EQ(reservation.x_residual, residual_t::literal(static_cast<int64_t>(quarter_raw)));
    EXPECT_EQ(sut.x_residual(), residual_t{});
    EXPECT_EQ(sut.y_residual(), residual_t{});
}

TEST_F(residual_accumulator_test_t, commit_updates_both_residuals)
{
    auto const reservation = sut.reserve(input_t::literal(quarter_raw), input_t::literal(-quarter_raw));
    ASSERT_TRUE(reservation.valid);

    sut.commit(reservation);

    EXPECT_EQ(sut.x_residual(), residual_t::literal(static_cast<int64_t>(quarter_raw)));
    EXPECT_EQ(sut.y_residual(), residual_t::literal(static_cast<int64_t>(-quarter_raw)));
}

TEST_F(residual_accumulator_test_t, half_ties_round_to_even_without_zero_input_oscillation)
{
    auto reservation = sut.reserve(input_t::literal(half_raw), input_t::literal(-half_raw));

    ASSERT_TRUE(reservation.valid);
    EXPECT_EQ(reservation.x, 0);
    EXPECT_EQ(reservation.y, 0);
    EXPECT_EQ(reservation.x_residual, residual_t::literal(static_cast<int64_t>(half_raw)));
    EXPECT_EQ(reservation.y_residual, residual_t::literal(static_cast<int64_t>(-half_raw)));
    sut.commit(reservation);

    reservation = sut.reserve(input_t{}, input_t{});
    ASSERT_TRUE(reservation.valid);
    EXPECT_EQ(reservation.x, 0);
    EXPECT_EQ(reservation.y, 0);
    EXPECT_EQ(reservation.x_residual, residual_t::literal(static_cast<int64_t>(half_raw)));
    EXPECT_EQ(reservation.y_residual, residual_t::literal(static_cast<int64_t>(-half_raw)));
}

TEST_F(residual_accumulator_test_t, axes_carry_independently)
{
    auto first = sut.reserve(input_t::literal(quarter_raw), input_t::literal(-quarter_raw));
    ASSERT_TRUE(first.valid);
    sut.commit(first);

    auto second = sut.reserve(input_t::literal(quarter_raw), input_t::literal(-quarter_raw));
    ASSERT_TRUE(second.valid);

    EXPECT_EQ(second.x, 0);
    EXPECT_EQ(second.y, 0);
    EXPECT_EQ(second.x_residual, residual_t::literal(static_cast<int64_t>(half_raw)));
    EXPECT_EQ(second.y_residual, residual_t::literal(static_cast<int64_t>(-half_raw)));
}

TEST_F(residual_accumulator_test_t, committed_carry_preserves_accumulated_displacement)
{
    auto emitted = int64_t{};

    for (auto i = int_t{0}; i != 4; ++i)
    {
        auto const reservation = sut.reserve(input_t::literal(quarter_raw), input_t{});
        ASSERT_TRUE(reservation.valid);
        emitted += reservation.x;
        sut.commit(reservation);
    }

    EXPECT_EQ(emitted, 1);
    EXPECT_EQ(sut.x_residual(), residual_t{});
}

TEST_F(residual_accumulator_test_t, mixed_signed_sequence_satisfies_telescoping_invariant)
{
    auto const inputs = std::array{quarter_raw, half_raw, -quarter_raw * 3, -half_raw, quarter_raw * 5};
    auto input_sum = int128_t{};
    auto output_sum = int128_t{};

    for (auto const raw : inputs)
    {
        auto const reservation = sut.reserve(input_t::literal(raw), input_t{});
        ASSERT_TRUE(reservation.valid);
        input_sum += raw;
        output_sum += static_cast<int128_t>(reservation.x) * (int128_t{1} << input_t::frac_bits);
        sut.commit(reservation);
    }

    EXPECT_EQ(output_sum + sut.x_residual().value, input_sum);
}

TEST_F(residual_accumulator_test_t, unrepresentable_integer_output_returns_invalid_reservation_without_state_change)
{
    auto const too_large = input_t{int128_t{max<int32_t>()} + 1};
    auto const reservation = sut.reserve(too_large, input_t{});

    EXPECT_FALSE(reservation.valid);
    EXPECT_EQ(sut.x_residual(), residual_t{});
    EXPECT_EQ(sut.y_residual(), residual_t{});
}

} // namespace
} // namespace crv::pipeline
