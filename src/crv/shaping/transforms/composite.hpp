// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::shaping::transforms {

/// composes two functions: outer(inner(value)) -> value_t
template <typename inner_t, typename outer_t> struct composite_t
{
    inner_t inner;
    outer_t outer;

    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t value) const noexcept -> value_t
    {
        return outer(inner(value));
    }
};

} // namespace crv::shaping::transforms
