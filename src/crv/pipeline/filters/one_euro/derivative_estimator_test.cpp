// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "derivative_estimator.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::pipeline::filters::one_euro {
namespace {

struct pipeline_filters_one_euro_derivative_estimator_test_t : Test
{
    using x_t = fixed_t<int32_t, 20>;
    using dx_t = fixed_t<int32_t, 16>;
    using reciprocal_dt_ms_t = fixed_t<uint32_t, 18>;
    using alpha_t = fixed_t<uint64_t, 64>;

    struct mock_ema_accumulator_t
    {
        virtual ~mock_ema_accumulator_t() = default;

        MOCK_METHOD(dx_t, call, (dx_t input, alpha_t alpha));
        MOCK_METHOD(dx_t, output, (), (const));
    };

    StrictMock<mock_ema_accumulator_t> mock_ema_accumulator;

    struct ema_accumulator_t
    {
        mock_ema_accumulator_t* mock = nullptr;

        auto operator()(dx_t input, alpha_t alpha) noexcept -> dx_t { return mock->call(input, alpha); }

        auto output() const noexcept -> dx_t { return mock->output(); }
    };

    using sut_t = derivative_estimator_t<x_t, dx_t, ema_accumulator_t>;

    static constexpr auto reciprocal_one = reciprocal_dt_ms_t{1};
    static constexpr auto reciprocal_half = reciprocal_one >> 1;
    static constexpr auto reciprocal_quarter = reciprocal_half >> 1;
    static constexpr auto reciprocal_thirty_two = reciprocal_dt_ms_t{32};
};

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, output_returns_directly_from_ema)
{
    auto sut = sut_t{{&mock_ema_accumulator}};

    EXPECT_CALL(mock_ema_accumulator, output()).WillOnce(Return(dx_t{37}));

    EXPECT_EQ(sut.output(), dx_t{37});
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, calculates_derivative_routes_alpha_and_returns_ema_output)
{
    auto sut = sut_t{{&mock_ema_accumulator}, x_t{4}};

    auto const x = x_t{12};
    auto const alpha = alpha_t::literal(uint64_t{1} << 62);
    auto const filtered_dx = dx_t{37};

    // (12 - 4) * 0.25 = 2
    EXPECT_CALL(mock_ema_accumulator, call(dx_t{2}, alpha)).WillOnce(Return(filtered_dx));

    EXPECT_EQ(sut(x, reciprocal_quarter, alpha), filtered_dx);
    EXPECT_EQ(sut.prev(), x);
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, calculates_negative_derivative)
{
    auto sut = sut_t{{&mock_ema_accumulator}, x_t{12}};

    auto const x = x_t{4};
    auto const alpha = alpha_t{};
    auto const filtered_dx = dx_t{37};

    // (4 - 12) * 0.25 = -2
    EXPECT_CALL(mock_ema_accumulator, call(dx_t{-2}, alpha)).WillOnce(Return(filtered_dx));

    EXPECT_EQ(sut(x, reciprocal_quarter, alpha), filtered_dx);
    EXPECT_EQ(sut.prev(), x);
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, each_call_uses_the_immediately_previous_raw_sample)
{
    auto sut = sut_t{{&mock_ema_accumulator}};

    auto const alpha_first = alpha_t{};
    auto const alpha_second = alpha_t::literal(uint64_t{1} << 63);

    {
        InSequence sequence;

        EXPECT_CALL(mock_ema_accumulator, call(dx_t{10}, alpha_first)).WillOnce(Return(dx_t{111}));

        // must use 13 - 10, not 13 - 0 and not the EMA's returned 111
        EXPECT_CALL(mock_ema_accumulator, call(dx_t{3}, alpha_second)).WillOnce(Return(dx_t{222}));
    }

    EXPECT_EQ(sut(x_t{10}, reciprocal_one, alpha_first), dx_t{111});
    EXPECT_EQ(sut(x_t{13}, reciprocal_one, alpha_second), dx_t{222});
    EXPECT_EQ(sut.prev(), x_t{13});
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, equal_physical_derivatives_produce_equal_ema_inputs)
{
    auto one_ms = sut_t{{&mock_ema_accumulator}};
    auto two_ms = sut_t{{&mock_ema_accumulator}};

    auto const alpha = alpha_t{};

    {
        InSequence sequence;

        // 1 unit over 1 ms
        EXPECT_CALL(mock_ema_accumulator, call(dx_t{1}, alpha)).WillOnce(Return(dx_t{}));

        // 2 units over 2 ms
        EXPECT_CALL(mock_ema_accumulator, call(dx_t{1}, alpha)).WillOnce(Return(dx_t{}));
    }

    one_ms(x_t{1}, reciprocal_one, alpha);
    two_ms(x_t{2}, reciprocal_half, alpha);
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, derivative_conversion_uses_nearest_even_for_positive_ties)
{
    auto lower_tie = sut_t{{&mock_ema_accumulator}};
    auto upper_tie = sut_t{{&mock_ema_accumulator}};

    auto const alpha = alpha_t{};

    {
        InSequence sequence;
        EXPECT_CALL(mock_ema_accumulator, call(dx_t::literal(int32_t{2}), alpha)).WillOnce(Return(dx_t{}));
        EXPECT_CALL(mock_ema_accumulator, call(dx_t::literal(int32_t{2}), alpha)).WillOnce(Return(dx_t{}));
    }

    // distinguish nearest rounding from truncation: x raw 24 -> dx raw 1.5 -> 2
    lower_tie(x_t::literal(int32_t{24}), reciprocal_one, alpha);

    // distinguish nearest-even from nearest-away: x raw 40 -> dx raw 2.5 -> 2
    upper_tie(x_t::literal(int32_t{40}), reciprocal_one, alpha);
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, derivative_conversion_uses_nearest_even_for_negative_ties)
{
    auto lower_tie = sut_t{{&mock_ema_accumulator}};
    auto upper_tie = sut_t{{&mock_ema_accumulator}};

    auto const alpha = alpha_t{};

    {
        InSequence sequence;
        EXPECT_CALL(mock_ema_accumulator, call(dx_t::literal(int32_t{-2}), alpha)).WillOnce(Return(dx_t{}));
        EXPECT_CALL(mock_ema_accumulator, call(dx_t::literal(int32_t{-2}), alpha)).WillOnce(Return(dx_t{}));
    }

    // distinguish nearest rounding from truncation: x raw -24 -> dx raw -1.5 -> -2
    lower_tie(x_t::literal(int32_t{-24}), reciprocal_one, alpha);

    // distinguish nearest-even from nearest-away: x raw -40 -> dx raw -2.5 -> -2
    upper_tie(x_t::literal(int32_t{-40}), reciprocal_one, alpha);
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, delta_subtraction_saturates_min)
{
    auto sut = sut_t{{&mock_ema_accumulator}, max<x_t>()};

    auto const alpha = alpha_t{};

    // min(x_t) - max(x_t) saturates to min(x_t)
    //
    // min(x_t) is exactly -2048 in this format, which remains representable in dx_t. Output conversion itself does not
    // saturate.
    EXPECT_CALL(mock_ema_accumulator, call(dx_t{-2048}, alpha)).WillOnce(Return(dx_t{}));

    sut(min<x_t>(), reciprocal_one, alpha);

    EXPECT_EQ(sut.prev(), min<x_t>());
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, delta_subtraction_saturates_max)
{
    auto sut = sut_t{{&mock_ema_accumulator}, min<x_t>()};

    auto const alpha = alpha_t{};

    // max(x_t) - min(x_t) saturates to max(x_t)
    //
    // max(x_t) is one x_t ULP below 2048. Narrowing with rne produces exactly 2048 in dx_t. Output conversion itself
    // does not saturate.
    EXPECT_CALL(mock_ema_accumulator, call(dx_t{2048}, alpha)).WillOnce(Return(dx_t{}));

    sut(max<x_t>(), reciprocal_one, alpha);

    EXPECT_EQ(sut.prev(), max<x_t>());
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, scaled_derivative_saturates_min)
{
    auto sut = sut_t{{&mock_ema_accumulator}};

    auto const alpha = alpha_t{};

    // min(x_t) - 0 = -2048 is representable. Scaling by 32 produces -65536, outside dx_t's range.
    EXPECT_CALL(mock_ema_accumulator, call(min<dx_t>(), alpha)).WillOnce(Return(dx_t{}));

    sut(min<x_t>(), reciprocal_thirty_two, alpha);
}

TEST_F(pipeline_filters_one_euro_derivative_estimator_test_t, scaled_derivative_saturates_max)
{
    auto sut = sut_t{{&mock_ema_accumulator}};

    auto const alpha = alpha_t{};

    // max(x_t) - 0 ~= 2048 is representable. Scaling by 32 produces approximately 65536, outside dx_t's range.
    EXPECT_CALL(mock_ema_accumulator, call(max<dx_t>(), alpha)).WillOnce(Return(dx_t{}));

    sut(max<x_t>(), reciprocal_thirty_two, alpha);
}

} // namespace
} // namespace crv::pipeline::filters::one_euro
