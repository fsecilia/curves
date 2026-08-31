// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

namespace crv::shaping {

/// composes a limiter over a nested curve
template <typename t_limiter_t, typename t_nested_curve_t> class limited_curve_t
{
public:
    using limiter_t = t_limiter_t;
    using nested_curve_t = t_nested_curve_t;
    using scalar_t = nested_curve_t::scalar_t;

    constexpr limited_curve_t(limiter_t limiter, nested_curve_t curve) noexcept
        : limiter_{std::move(limiter)}, curve_{std::move(curve)}
    {}

    template <typename input_t> [[nodiscard]] auto operator()(input_t input) const noexcept -> input_t
    {
        return limiter_.apply(curve_, input);
    }

private:
    limiter_t limiter_;
    nested_curve_t curve_;
};

} // namespace crv::shaping
