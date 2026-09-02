// SPDX-License-Identifier: MIT

/// \file
/// \brief authored curve interpretation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv::model {

/// identifies authored quantity represented by a curve
enum class curve_interpretation_t
{
    gain,
    sensitivity,
};

} // namespace crv::model
