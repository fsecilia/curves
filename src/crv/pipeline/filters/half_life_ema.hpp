// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline {

/// exponential moving average parameterized by half-life
///
/// This implementation uses a backward-Euler smoothing factor: alpha = (duration*ln(2))/(half_life + duration*ln(2))
template <is_fixed sample_t, is_fixed time_t>
    requires(is_signed_v<sample_t> && !is_signed_v<time_t>)
class half_life_ema_t
{
public:
    constexpr auto output() const noexcept -> sample_t { return output_; }

    constexpr auto operator()(sample_t input, time_t half_life, time_t duration) noexcept -> sample_t
    {
        assert(half_life > time_t{0} && "half_life_ema_t: half-life must be positive");

        auto const scaled_duration = multiply<time_t, shifter>(duration, ln2);
        assert(
            scaled_duration <= max<time_t>() - half_life && "half_life_ema_t: half-life plus scaled duration overflow");

        auto const alpha = divide<smoothing_factor_t>(scaled_duration, half_life + scaled_duration);
        auto const error = saturating_sub(input, output_);
        auto const correction = multiply<sample_t, shifter>(alpha, error);

        output_ += correction;
        return output_;
    }

private:
    using smoothing_factor_t = fixed_t<typename time_t::value_t, time_t::container_bits>;

    static_assert(sizeof(typename time_t::value_t) <= sizeof(uint64_t),
        "half_life_ema_t: time type wider than supported divider");

    static constexpr auto shifter = shifter_t<rounding_modes::shr::fast::nearest_away>{};
    static constexpr auto ln2
        = smoothing_factor_t::template convert<shifter>(fixed_t<uint64_t, 64>::literal(12786308645202655660ULL));

    sample_t output_{};
};

} // namespace crv::pipeline
