// SPDX-License-Identifier: MIT

/// \file
/// \brief config app qt entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/ui/model/config.hpp>
#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

namespace crv {

static auto main(int argc, char* argv[]) -> int
{
    QGuiApplication::setApplicationName("curves");
    QGuiApplication::setOrganizationName("");

    auto gui_app = QGuiApplication{argc, argv};

    return EXIT_SUCCESS;
}

} // namespace crv

auto main(int argc, char* argv[]) -> int
{
    return crv::main(argc, argv);
}
