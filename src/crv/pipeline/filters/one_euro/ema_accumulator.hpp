// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline::filters::one_euro {

/// exponential moving average low-pass smoother over signed fixed-point signals
///
/// Applies y[n] = y[n - 1] + alpha[n](x[n] - y[n - 1]) with alpha supplied by caller.
template <is_fixed sample_t, shifter_t shifter = {}>
    requires(is_signed_v<sample_t>)
class ema_accumulator_t
{
public:
    constexpr ema_accumulator_t() = default;
    constexpr explicit ema_accumulator_t(sample_t initial) noexcept : output_{initial} {}

    constexpr auto output() const noexcept -> sample_t { return output_; }

    template <is_fixed_frac smoothing_factor_t>
    constexpr auto operator()(sample_t input, smoothing_factor_t alpha) noexcept -> sample_t
        requires(!is_signed_v<smoothing_factor_t>)
    {
        auto const error = saturating_sub(input, output_);
        auto const correction = multiply<sample_t, shifter>(alpha, error);

        // cannot overflow - correction has error's sign and cannot round past it; sum stays between output and error
        output_ += correction;
        return output_;
    }

private:
    sample_t output_{};
};

} // namespace crv::pipeline::filters::one_euro
