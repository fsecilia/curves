// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "half_life_ema.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct half_life_ema_test_t : Test
{
    using sample_t = fixed_t<int32_t, 16>;
    using time_t = fixed_t<uint64_t, 0>;
    using sut_t = half_life_ema_t<sample_t, time_t>;

    template <typename sample_t, typename time_t>
    static constexpr auto is_half_life_ema = requires { typename half_life_ema_t<sample_t, time_t>; };

    static_assert(is_half_life_ema<fixed_t<int32_t, 16>, fixed_t<uint64_t, 0>>);
    static_assert(!is_half_life_ema<fixed_t<uint32_t, 16>, fixed_t<uint64_t, 0>>);
    static_assert(!is_half_life_ema<fixed_t<int32_t, 16>, fixed_t<int64_t, 0>>);
    static_assert(!is_half_life_ema<int32_t, fixed_t<uint64_t, 0>>);
    static_assert(!is_half_life_ema<fixed_t<int32_t, 16>, uint64_t>);

    static_assert(sizeof(sut_t) == sizeof(sample_t));
    static_assert(sut_t{}.output() == sample_t{});

    static constexpr auto zero_duration_does_not_advance = [] {
        auto sut = sut_t{};
        return sut(sample_t{100}, time_t{1'500'000}, time_t{0}) == sample_t{};
    }();
    static_assert(zero_duration_does_not_advance);

    static constexpr auto zero_stays_zero = [] {
        auto sut = sut_t{};
        return sut(sample_t{}, time_t{1'500'000}, time_t{250'000}) == sample_t{};
    }();
    static_assert(zero_stays_zero);

    static constexpr auto positive_step_moves_toward_input = [] {
        auto sut = sut_t{};
        auto const input = sample_t{100};
        auto const output = sut(input, time_t{1'500'000}, time_t{250'000});
        return sample_t{} < output && output < input && output == sut.output();
    }();
    static_assert(positive_step_moves_toward_input);

    static constexpr auto negative_step_moves_toward_input = [] {
        auto sut = sut_t{};
        auto const input = sample_t{-100};
        auto const output = sut(input, time_t{1'500'000}, time_t{250'000});
        return input < output && output < sample_t{};
    }();
    static_assert(negative_step_moves_toward_input);

    static constexpr auto half_ties_round_away = [] {
        using tie_sample_t = fixed_t<int16_t, 8>;
        using tie_time_t = fixed_t<uint16_t, 0>;
        using tie_sut_t = half_life_ema_t<tie_sample_t, tie_time_t>;

        auto positive = tie_sut_t{};
        auto negative = tie_sut_t{};

        // duration*ln(2) rounds to 1, so half_life=1 gives alpha=0.5 exactly
        return positive(tie_sample_t::literal(1), tie_time_t{1}, tie_time_t{1}) == tie_sample_t::literal(1)
            && negative(tie_sample_t::literal(-1), tie_time_t{1}, tie_time_t{1}) == tie_sample_t::literal(-1);
    }();
    static_assert(half_ties_round_away);

    static constexpr auto repeated_updates_converge_without_overshoot = [] {
        auto sut = sut_t{};
        auto const input = sample_t{100};
        auto previous = sample_t{};

        for (auto i = int_t{0}; i != 100; ++i)
        {
            auto const output = sut(input, time_t{1'500'000}, time_t{250'000});
            if (!(previous <= output && output <= input)) return false;
            previous = output;
        }

        return previous > sample_t{99};
    }();
    static_assert(repeated_updates_converge_without_overshoot);

    static constexpr auto larger_duration_moves_further = [] {
        auto short_sut = sut_t{};
        auto long_sut = sut_t{};
        auto const input = sample_t{100};
        auto const half_life = time_t{1'500'000};

        return short_sut(input, half_life, time_t{125'000}) < long_sut(input, half_life, time_t{1'000'000});
    }();
    static_assert(larger_duration_moves_further);

    static constexpr auto larger_half_life_moves_less = [] {
        auto short_sut = sut_t{};
        auto long_sut = sut_t{};
        auto const input = sample_t{100};
        auto const duration = time_t{250'000};

        return short_sut(input, time_t{1'000'000}, duration) > long_sut(input, time_t{2'000'000}, duration);
    }();
    static_assert(larger_half_life_moves_less);

    static constexpr auto four_khz_one_point_five_ms_half_life = [] {
        auto sut = sut_t{};

        // alpha = (0.25*ln(2))/(1.5 + 0.25*ln(2)) ~= 0.103561
        return sut(sample_t{100}, time_t{1'500'000}, time_t{250'000}) == sample_t::literal(678696);
    }();
    static_assert(four_khz_one_point_five_ms_half_life);
};

} // namespace
} // namespace crv
