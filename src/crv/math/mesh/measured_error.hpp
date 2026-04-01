// SPDX-License-Identifier: MIT

/// \file
/// \brief error metric result
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv {

/// standardized output from an error metric
///
/// This type encapsulates the result of measuring error using a formal metric. It holds the magnitude of the error and
/// position of the sample that generated it.
template <typename real_t> struct measured_error_t
{
    real_t position;
    real_t magnitude;

    auto operator<=>(measured_error_t const& src) const noexcept -> auto = default;
    auto operator==(measured_error_t const& src) const noexcept -> bool  = default;
};

/// orders measured errors by magnitude
struct measured_error_pred_t
{
    static constexpr auto operator()(auto const& left, auto const& right) noexcept -> bool
    {
        return left.magnitude < right.magnitude;
    }
};

} // namespace crv
