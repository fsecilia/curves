// SPDX-License-Identifier: MIT

/// \file
/// \brief typed bools describing whether to saturate or not in a self-documenting way
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv {

enum class overflow_policy_t
{
    saturate,
    wrap
};

} // namespace crv
