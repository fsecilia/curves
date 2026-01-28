// SPDX-License-Identifier: MIT
/*!
  \file
  \brief fundamental integer type traits

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <cassert>
#include <utility>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// int_cast
// --------------------------------------------------------------------------------------------------------------------

//! asserts that from is in the representable range of to_t
template <integral to_t, integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(std::in_range<to_t>(from) && "out of range integer cast");
    return static_cast<to_t>(from);
}

} // namespace crv
