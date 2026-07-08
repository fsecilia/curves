// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline::filters::one_euro {

/// exponential moving average low-pass section over fixed-point signals
///
/// This is a first-order low-pass smoothing section, a one-pole smoother. It applies the recurrence
///
///     `y[n] = y[n − 1] + α[n](x[n] − y[n−1])`
///
/// where `x[n]` is the current input sample, `y[n]` is the filtered output, and `a[n]` is a smoothing factor in [0, 1].
/// An `a[n]` of 0 holds the previous output; an `a[n]` of 1 follows the input immediately. The difference form is used
/// to preserve precision when the input is close to the current output.
///
/// \tparam sample_t fixed-point signal type
/// \tparam shifter_t shifter used during narrowing; defaults to using rne
template <is_fixed sample_t, shifter_t shifter = {}> class ema_section_t
{
public:
    constexpr ema_section_t() = default;
    constexpr explicit ema_section_t(sample_t initial) noexcept : output_{initial} {}

    /// \returns current filter output
    constexpr auto output() const noexcept -> sample_t { return output_; }

    /// applies one smoothing step
    ///
    /// Wide intermediates are narrowed using the configured shifter with saturation.
    ///
    /// \param input current input sample, x[n]
    /// \tparam smoothing_factor_t unsigned fractional in [0, 1]; higher is more responsive
    /// \param smoothing_factor smoothing factor a[n]
    /// \returns current filtered output, y[n]
    /// \pre smoothing_factor >= 0
    /// \pre smoothing_factor <= 1
    template <is_fixed smoothing_factor_t>
    constexpr auto operator()(sample_t input, smoothing_factor_t smoothing_factor) noexcept -> sample_t
        requires(!crv::is_signed_v<typename smoothing_factor_t::value_t>
            && smoothing_factor_t::frac_bits == sizeof(typename smoothing_factor_t::value_t) * CHAR_BIT)
    {
        assert(smoothing_factor >= smoothing_factor_t{});

        auto const error = saturating_sub(input, output_);
        auto const correction = multiply<sample_t, shifter>(smoothing_factor, error);
        output_ += correction;
        return output_;
    }

private:
    sample_t output_{};
};

} // namespace crv::pipeline::filters::one_euro
