// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cmath>
#include <concepts>

namespace crv::model {

/// input domain containing every finite value
template <std::floating_point scalar_t> struct unbounded_domain_t
{
    [[nodiscard]] constexpr auto contains(scalar_t value) const noexcept -> bool { return std::isfinite(value); }
};

} // namespace crv::model
