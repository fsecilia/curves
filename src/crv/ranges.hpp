// SPDX-License-Identifier: MIT

/// \file
/// \brief common range definitions missing from the stdlib
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <ranges>

namespace crv {

template <typename range_t, typename value_t>
concept compatible_range
    = std::ranges::range<range_t> && std::convertible_to<std::ranges::range_value_t<range_t>, value_t>;

template <typename range_t, typename value_t>
concept same_range = std::ranges::range<range_t> && std::same_as<value_t, std::ranges::range_value_t<range_t>>;

} // namespace crv
