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

/// transforms input so output starts off horizontal then smoothly transitions to original function with lag
///
/// An offset is an optional delay of the input with a horizontal line at a configurable height for a configurable
/// width, then a smooth transition into the original function with a configurable width, then the original function
/// lagged by both the horizontal section width and some fraction of the transition.
///
/// This type shapes the offset via function composition. It controls the rate at which the composed function advances,
/// starting paused, then smoothly returning to full running speed via the given transition.
///
/// The transform is identity when width == 0. When width == 0, start must also be 0.
///
/// \invariant (start == 0 && width == 0) || (start >= 0 && min_width > 0 && width >= min_width)
template <std::floating_point t_real_t, typename transition_t> class offset_t
{
public:
    using real_t = t_real_t;

    constexpr offset_t() = default;
    explicit constexpr offset_t(transition_t transition) noexcept : offset_t{0.0, 0.0, std::move(transition)} {}

    constexpr offset_t(real_t start, real_t width, real_t min_width, transition_t transition) noexcept
        : start_{start}, width_{width}, transition_{std::move(transition)}
    {
        static_cast<void>(min_width);
        assert(((start_ == real_t{0} && width_ == real_t{0})
                   || (start_ >= real_t{0} && min_width > real_t{0} && width_ >= min_width))
            && "offset_t: invariant violated");
        rwidth_ = width_ > real_t{0} ? real_t{1} / width_ : real_t{0};

        lag_ = start_ + width_ * (real_t{1} - transition_(real_t{1}));
    }

    // applys delay, transition, or lag, piecewise
    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        if (width_ == real_t{0}) return input;

        auto const x = primal(input);
        if (x <= start_) return value_t{0};
        if (x >= start_ + width_) return input - lag_;
        return width_ * transition_((input - start_) * rwidth_);
    }

private:
    real_t start_{};
    real_t width_{};
    real_t rwidth_{};
    real_t lag_{};

    // transition is scaled uniformly by width to keep its aspect ratio
    [[no_unique_address]] transition_t transition_{};
};

} // namespace crv::shaping::transforms
