// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv::signal_chain::stages {

/// affine transform on input
///
/// \invariant #scale > 0.0
template <std::floating_point real_t, typename prev_t> class input_affine_t
{
public:
    constexpr input_affine_t(real_t scale, real_t shift, prev_t prev) noexcept
        : scale_{scale}, shift_{shift}, prev_{std::move(prev)}
    {
        assert(scale > real_t{0});
    }

    /// applies forward transform prev.forward(scale(x - shift))
    template <typename value_t> [[nodiscard]] constexpr auto forward(value_t x) const noexcept -> value_t
    {
        return prev_.forward(scale_ * (x - shift_));
    }

    /// applies inverse transform prev.inverse(y)/scale + shift
    template <typename value_t> [[nodiscard]] constexpr auto inverse(value_t y) const noexcept -> value_t
    {
        assert(scale_ > real_t{0});

        // y = prev.forward(scale*(x - shift))
        // prev.inverse(y) = scale*(x - shift)
        // prev.inverse(y)/scale = x - shift
        // prev.inverse(y)/scale + shift = x
        return prev_.inverse(y) / scale_ + shift_;
    }

private:
    real_t scale_;
    real_t shift_;
    prev_t prev_;
};

} // namespace crv::signal_chain::stages
