// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/scalar_traits.hpp>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv::shaping::transforms {

/// transforms input so output matches original function then smoothly transitions to a constant limit
///
/// A limit starts with the original input up to a configurable start point, then smoothly decelerates to a hard cap
/// over a configurable width. It includes a blend factor to gently roll the engagement of the limiter on rather than
/// jumping from 0% to 100%.
///
/// This type shapes the limit via function composition. It controls the rate at which the composed function advances,
/// starting at full rate, then smoothly pausing via the given transition.
///
/// The transform is identity when blend == 0.
///
/// \invariant width > 0 && 0 <= blend && blend <= 1
template <std::floating_point t_real_t, typename transition_t> class limit_t
{
public:
    using real_t = t_real_t;

    constexpr limit_t(real_t start, real_t width, real_t min_width, real_t blend, transition_t transition) noexcept
        : start_{start}, width_{width}, blend_{blend}, transition_{std::move(transition)}
    {
        static_cast<void>(min_width);
        assert(blend_ >= real_t{0} && blend_ <= real_t{1} && "limit_t: blend must be in [0, 1]");
        assert(min_width > real_t{0} && "limit_t: min_width must be greater than 0");
        assert(width_ >= min_width && "limit_t: width must be greater than min_width");
        rwidth_ = real_t{1} / width_;

        transition_max_ = transition_(real_t{1});
        input_max_ = start_ + width_ * transition_max_;
    }

    // applies start, transition, or limit, piecewise, then applies blend
    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        if (blend_ <= real_t{0}) return input;

        auto const x = primal(input);
        if (x <= start_) return input;

        value_t input_limited;
        if (x >= start_ + width_) input_limited = value_t{input_max_};
        else
        {
            auto const t = (input - start_) * rwidth_;
            input_limited = start_ + width_ * (transition_max_ - transition_(value_t{1} - t));
        }

        if (blend_ >= real_t{1}) return input_limited;

        return (real_t{1} - blend_) * input + blend_ * input_limited;
    }

private:
    real_t start_{};
    real_t width_{};
    real_t blend_{};
    real_t rwidth_{};
    real_t transition_max_{};
    real_t input_max_{};

    // transition is scaled uniformly by width to keep its aspect ratio
    [[no_unique_address]] transition_t transition_{};
};

} // namespace crv::shaping::transforms
