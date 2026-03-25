// SPDX-License-Identifier: MIT

/// \file
/// \brief division results
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <compare>

namespace crv::division {

/// result of division in full (quotient, remainder) form
template <typename narrow_t> struct qr_pair_t
{
    narrow_t quotient;
    narrow_t remainder;

    auto operator<=>(qr_pair_t const&) const noexcept -> auto = default;
    auto operator==(qr_pair_t const&) const noexcept -> bool  = default;
};

} // namespace crv::division
