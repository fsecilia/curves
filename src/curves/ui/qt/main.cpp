// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <QApplication>

auto main(int argc, char* argv[]) -> int {
  auto app = QApplication{argc, argv};

  auto main_window = MainWindow{};
  main_window.show();

  return app.exec();
}
