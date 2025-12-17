// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <curves/config/profile.hpp>
#include <curves/math/spline.hpp>
#include <curves/math/transfer_function.hpp>
#include <curves/modes/synchronous.hpp>
#include <QApplication>
#include <QStandardPaths>

auto get_config_dir_path() -> std::string {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
      .toStdString();
}

auto create_default_profile() -> curves::Profile {
  auto profile = curves::Profile{};
  return profile;
}

auto main(int argc, char* argv[]) -> int {
  auto app = QApplication{argc, argv};
  QApplication::setApplicationName("curves");
  QApplication::setOrganizationName("");

  const auto config_dir_path = get_config_dir_path();
  auto default_profile = create_default_profile();

  auto main_window = MainWindow{};

  main_window.prepopulateCurveParameterWidgets(10);

  auto sensitivity = curves::modes::synchronous::Curve{
      0.433012701892L, 17.3205080757L, 5.33L, 28.3L, 0.5L};
  main_window.setSpline(std::make_shared<curves_spline>(
      curves::spline::create_spline(curves::TransferFunction{sensitivity})));

  main_window.show();

  return app.exec();
}
