// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::pipeline::configuration {

enum class apply_mode_t : uint8_t
{
    bypassed,
    active,
};

} // namespace crv::pipeline::configuration
