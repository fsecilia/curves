// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <curves/math/spline.hpp>
#include <QApplication>

auto main(int argc, char* argv[]) -> int {
  auto app = QApplication{argc, argv};
  QApplication::setApplicationName("curves");
  QApplication::setOrganizationName("");

  auto main_window = MainWindow{};

  main_window.prepopulateCurveParameterWidgets(10);

  auto sensitivity = curves::SynchronousCurve{0.433012701892L, 17.3205080757L,
                                              5.33L, 28.3L, 0.5L};
  main_window.setSpline(
      std::make_shared<curves_spline>(curves::spline::create_spline(
          curves::TransferFunctionCurve{sensitivity})));

  main_window.show();

  return app.exec();
}
