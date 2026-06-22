// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv::signal_chain::stages {

/// affine transform on output
///
/// \invariant #scale > 0.0
template <std::floating_point real_t, typename prev_t> class output_affine_t
{
public:
    constexpr output_affine_t(real_t scale, real_t shift, prev_t prev) noexcept
        : scale_{scale}, shift_{shift}, prev_{std::move(prev)}
    {
        assert(scale > real_t{0});
    }

    /// applies forward transform scale*prev.forward(x) + shift
    template <typename value_t> [[nodiscard]] constexpr auto forward(value_t x) const noexcept -> value_t
    {
        return scale_ * prev_.forward(x) + shift_;
    }

    /// applies inverse transform prev.inverse((y - shift)/scale)
    template <typename value_t> [[nodiscard]] constexpr auto inverse(value_t y) const noexcept -> value_t
    {
        assert(scale_ > real_t{0});

        // y = scale*prev.forward(x) + shift
        // y - shift = scale*prev.forward(x)
        // (y - shift)/scale = prev.forward(x)
        // prev.inverse((y - shift)/scale) = x
        return prev_.inverse((y - shift_) / scale_);
    }

private:
    real_t scale_;
    real_t shift_;
    prev_t prev_;
};

} // namespace crv::signal_chain::stages
