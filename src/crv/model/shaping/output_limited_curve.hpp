// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

namespace crv::shaping {

/// composes an output limiter over a curve
template <typename t_limiter_t, typename t_curve_t> class output_limited_curve_t
{
public:
    constexpr output_limited_curve_t(t_limiter_t limiter, t_curve_t curve) noexcept
        : limiter_{std::move(limiter)}, curve_{std::move(curve)}
    {}

    template <typename input_t> [[nodiscard]] auto operator()(input_t input) const noexcept -> input_t
    {
        return limiter_.apply(curve_, input);
    }

private:
    t_limiter_t limiter_;
    t_curve_t curve_;
};

} // namespace crv::shaping
