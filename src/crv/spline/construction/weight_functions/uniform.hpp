// SPDX-License-Identifier: MIT

/// \file
/// \brief uniform perceptual domain error weight
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::spline::weight_functions {

/// neutral weighting, treats all domain coordinates equally
template <typename scalar_t> struct uniform_t
{
    constexpr auto operator()(scalar_t) const noexcept -> scalar_t { return 1.0; }
};

} // namespace crv::spline::weight_functions
