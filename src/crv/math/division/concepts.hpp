// SPDX-License-Identifier: MIT

/// \file
/// \brief division-specific concepts
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/int_traits.hpp>
#include <type_traits>

namespace crv::division {

// --------------------------------------------------------------------------------------------------------------------
// is_result
// --------------------------------------------------------------------------------------------------------------------

template <typename result_t>
concept is_result = requires(std::remove_cvref_t<result_t> result) {
    result.quotient;
    result.remainder;
    requires unsigned_integral<decltype(result.quotient)>;
    requires unsigned_integral<decltype(result.remainder)>;
};

// --------------------------------------------------------------------------------------------------------------------
// is_wide_divider
// --------------------------------------------------------------------------------------------------------------------

template <typename divider_t, typename narrow_t>
concept is_wide_divider = requires(divider_t const& divider, wider_t<narrow_t> dividend, narrow_t divisor) {
    { divider(dividend, divisor) } -> is_result;
};

} // namespace crv::division
