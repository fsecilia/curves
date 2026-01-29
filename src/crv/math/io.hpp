// SPDX-License-Identifier: MIT
/*!
    \file
    \brief io for math types

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <ostream>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// 128-bit types
// --------------------------------------------------------------------------------------------------------------------

auto operator<<(std::ostream& out, uint128_t src) -> std::ostream&;
auto operator<<(std::ostream& out, int128_t src) -> std::ostream&;

} // namespace crv
