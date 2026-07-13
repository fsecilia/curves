// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>
#include <utility>

namespace crv::pipeline::filters::one_euro {

/// estimates and smoothes true derivative dx = (x - x_prev)/dt_ms.
template <is_fixed t_x_t, is_fixed t_dx_t, typename t_derivative_ema_t>
    requires(is_signed_v<typename t_x_t::value_t> && is_signed_v<typename t_dx_t::value_t>)
class derivative_estimator_t
{
public:
    using x_t = t_x_t;
    using dx_t = t_dx_t;
    using derivative_ema_t = t_derivative_ema_t;

    constexpr derivative_estimator_t() noexcept = default;

    constexpr explicit derivative_estimator_t(derivative_ema_t derivative_ema, x_t initial = {}) noexcept
        : derivative_ema_{std::move(derivative_ema)}, x_prev_{initial}
    {}

    template <is_fixed reciprocal_dt_ms_t, is_fixed smoothing_factor_t>
    constexpr auto operator()(x_t x, reciprocal_dt_ms_t reciprocal_dt_ms, smoothing_factor_t alpha) noexcept -> dx_t
    {
        auto const delta_x = saturating_sub(x, x_prev_);
        x_prev_ = x;

        static constexpr auto rne = shifter_t<rounding_modes::shr::nearest_even>{};
        auto const dx = multiply<dx_t, rne>(delta_x, reciprocal_dt_ms);
        return derivative_ema_(dx, alpha);
    }

    constexpr auto output() const noexcept -> dx_t { return derivative_ema_.output(); }
    constexpr auto prev() const noexcept -> x_t { return x_prev_; }

private:
    derivative_ema_t derivative_ema_{};
    x_t x_prev_{};
};

} // namespace crv::pipeline::filters::one_euro
