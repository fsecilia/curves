// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::shaping::stages {

/// applies prev stage then transforms output
template <typename transform_t, typename prev_stage_t> struct output_t
{
    transform_t transform;
    prev_stage_t prev_stage;

    /// applies transform(prev_stage(input))
    template <typename value_t> [[nodiscard]] constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        return transform(prev_stage(input));
    }
};

} // namespace crv::shaping::stages
