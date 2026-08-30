// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <concepts>

namespace crv::shaping::transitions {

template <typename transition_t, typename scalar_t>
concept is_transition
    = std::floating_point<scalar_t> && requires(transition_t const& transition, scalar_t u, jet_t<scalar_t> jet) {
          { transition.value(u) } noexcept -> std::same_as<scalar_t>;
          { transition.derivative(u) } noexcept -> std::same_as<scalar_t>;
          { transition.antiderivative(u) } noexcept -> std::same_as<scalar_t>;
          { transition.value(jet) } noexcept -> std::same_as<jet_t<scalar_t>>;
          { transition.antiderivative(jet) } noexcept -> std::same_as<jet_t<scalar_t>>;
      };

} // namespace crv::shaping::transitions
