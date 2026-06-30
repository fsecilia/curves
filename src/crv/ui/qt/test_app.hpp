// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <QApplication>
#include <memory>
#include <string>

namespace crv {

/// meyers singleton app used during testing
class test_app_t
{
public:
    test_app_t(test_app_t const&) = delete;
    auto operator=(test_app_t const&) -> test_app_t& = delete;

    static auto instance() -> test_app_t const&;

private:
    test_app_t() = default;

    int argc = 1;
    std::string name = "gtest_runner";
    std::array<char*, 2> argv{name.data(), nullptr};
    std::unique_ptr<QApplication> app{std::make_unique<QApplication>(argc, argv.data())};
};

} // namespace crv
