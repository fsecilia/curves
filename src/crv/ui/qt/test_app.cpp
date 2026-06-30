// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "test_app.hpp"

namespace crv {

auto test_app_t::instance() -> test_app_t const&
{
    static auto const instance = test_app_t{};
    return instance;
}

} // namespace crv
