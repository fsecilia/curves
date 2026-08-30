// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <type_traits>
#include <vector>

namespace crv {

template <typename curve_t, typename real_t>
concept is_curve = std::floating_point<real_t> && requires(curve_t const& curve, real_t x) {
    typename curve_t::scalar_t;
    typename curve_t::domain_t;
    requires std::same_as<typename curve_t::scalar_t, real_t>;
    requires std::is_nothrow_invocable_v<curve_t, real_t>;
    { curve(x) } -> std::convertible_to<real_t>;
    { curve.domain() } -> std::same_as<typename curve_t::domain_t>;
    { curve.domain().contains(x) } -> std::same_as<bool>;
    { curve.critical_points() } -> std::same_as<std::vector<real_t>>;
};

} // namespace crv
