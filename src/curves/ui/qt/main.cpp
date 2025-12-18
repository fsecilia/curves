// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <curves/config/profile.hpp>
#include <curves/config/profile_store.hpp>
#include <curves/curves/synchronous.hpp>
#include <curves/math/spline.hpp>
#include <curves/math/transfer_function.hpp>
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <filesystem>

namespace curves {

auto get_config_dir_path() -> std::filesystem::path {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
      .toStdString();
}

auto report_config_file_parse_error(const toml::parse_error& err) -> void {
  std::ostringstream message;
  message << "Could not parse config file.\n\nIn file " << *err.source().path
          << ",\n"
          << err.source().begin << " to " << err.source().end << ":\n\n"
          << err.description();

  QMessageBox{QMessageBox::Critical, "Curves Configuration Error",
              QString::fromStdString(message.str())}
      .exec();
}

auto main(int argc, char* argv[]) -> int {
  auto app = QApplication{argc, argv};
  QApplication::setApplicationName("curves");
  QApplication::setOrganizationName("");

  auto profile_store = ProfileStore{get_config_dir_path() / "config.toml"};

  auto profile = Profile{};
  try {
    profile = profile_store.find_or_create();
  } catch (const toml::parse_error& err) {
    report_config_file_parse_error(err);
    return EXIT_FAILURE;
  }
  profile_store.save(profile);

  auto main_window = MainWindow{};

  main_window.prepopulateCurveParameterWidgets(10);

  auto sensitivity =
      SynchronousCurve{0.433012701892L, 17.3205080757L, 5.33L, 28.3L, 0.5L};
  main_window.setSpline(std::make_shared<curves_spline>(
      spline::create_spline(TransferFunction{sensitivity})));

  main_window.show();

  return app.exec();
}

}  // namespace curves

auto main(int argc, char* argv[]) -> int { return curves::main(argc, argv); }
