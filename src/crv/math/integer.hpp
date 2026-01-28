// SPDX-License-Identifier: MIT
/*!
  \file
  \brief safe integer casts

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv {

//! asserts that from is in the representable range of to_t
template <std::integral to_t, std::integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(std::in_range<to_t>(from) && "out of range integer cast");
    return static_cast<to_t>(from);
}

} // namespace crv
