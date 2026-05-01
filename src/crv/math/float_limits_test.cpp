// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "float_limits.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

template <typename value_t> constexpr auto test_type_limits() noexcept -> void
{
    static_assert(max<value_t>() == std::numeric_limits<value_t>::max());
    static_assert(min<value_t>() == std::numeric_limits<value_t>::lowest());
}

template <typename... values_t> constexpr auto test_types_limits() noexcept -> void
{
    (..., test_type_limits<values_t>());
}

template <typename... values_t> [[maybe_unused]] constexpr auto test_all_types_limits() noexcept -> void
{
    test_types_limits<float32_t, float64_t>();
}

} // namespace
} // namespace crv
