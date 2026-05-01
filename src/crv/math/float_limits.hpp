// SPDX-License-Identifier: MIT

/// \file
/// \brief simplified numeric_limits for common values
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/limits.hpp>
#include <limits>

namespace crv {

template <typename value_t, int_t size> struct min_max_by_size_t<value_t, size, true, true>
{
    static constexpr auto max = std::numeric_limits<value_t>::max();
    static constexpr auto min = std::numeric_limits<value_t>::lowest();
};

} // namespace crv
