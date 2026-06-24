// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <type_traits>

namespace crv {

template <typename curve_t, typename real_t>
concept is_curve = std::floating_point<real_t> && std::is_nothrow_invocable_v<curve_t, real_t>
    && requires(curve_t const& curve, real_t x) {
           { curve(x) } -> std::convertible_to<real_t>;
       };

} // namespace crv
