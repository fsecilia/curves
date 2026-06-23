// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::signal_chain::stages {

/// transforms input then applies prev stage
template <typename transform_t, typename prev_stage_t> struct input_t
{
    transform_t transform;
    prev_stage_t prev_stage;

    /// applies prev_stage(transform(input))
    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return prev_stage(transform(input));
    }
};

} // namespace crv::signal_chain::stages
