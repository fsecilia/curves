// SPDX-License-Identifier: MIT

/// \file
/// \brief config app qt entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/model/config.hpp>
#include <crv/serialization/exceptions.hpp>
#include <crv/serialization/toml/toml.hpp>
#include <crv/ui/qt/app.hpp>
#include <crv/ui/qt/property_model.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <cstdlib>

namespace crv {
namespace {

auto main(int argc, char* argv[]) -> int
{
    auto app = app_t{};
    if (!app.initialize(argc, argv)) return EXIT_FAILURE;
    return app.run();
}

} // namespace
} // namespace crv

auto main(int argc, char* argv[]) -> int
{
    return crv::main(argc, argv);
}
