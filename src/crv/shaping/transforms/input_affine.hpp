// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>

namespace crv::signal_chain::transforms {

/// affine transform on input
///
/// \invariant transform must be invertible; #scale_ > 0.0
template <std::floating_point real_t> class input_affine_t
{
public:
    constexpr input_affine_t(real_t scale, real_t shift) noexcept : scale_{scale}, shift_{shift}
    {
        assert(scale > real_t{0});
    }

    /// applies scale*(input - shift)
    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return scale_ * (input - shift_);
    }

private:
    real_t scale_;
    real_t shift_;
};

} // namespace crv::signal_chain::transforms
