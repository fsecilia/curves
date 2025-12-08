// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <curves/spline.hpp>
#include <QApplication>

auto main(int argc, char* argv[]) -> int {
  auto app = QApplication{argc, argv};

  auto main_window = MainWindow{};

  main_window.prepopulateCurveParameterWidgets(10);

  auto sensitivity = [](long double x) {
    return curves::synchronous(x, 0.433012701892L, 8.3L, 17.3205080757L, 2.33L,
                               0.5L);
  };
  const auto x_max = 256.0L;
  main_window.setSpline(std::make_shared<curves_spline_table>(
      curves::generate_table_from_sensitivity(sensitivity, x_max)));

  main_window.show();

  return app.exec();
}
