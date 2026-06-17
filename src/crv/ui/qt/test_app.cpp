// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "test_app.hpp"

namespace crv {

test_app_t::test_app_t() : app{std::make_unique<QApplication>(argc, argv.data())}
{}

} // namespace crv
