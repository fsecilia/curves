// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <type_traits>

namespace crv {

template <typename enum_t>
concept is_enum = std::is_enum_v<enum_t>;

} // namespace crv
