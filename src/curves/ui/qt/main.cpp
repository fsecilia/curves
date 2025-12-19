// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <curves/config/profile.hpp>
#include <curves/config/profile_store.hpp>
#include <curves/ui/model/view_model.hpp>
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

  auto profile_store_path = get_config_dir_path() / "config.toml";
  auto profile_store = std::make_shared<ProfileStore>(profile_store_path);

  auto profile = Profile{};
  try {
    profile = profile_store->find_or_create();
  } catch (const toml::parse_error& err) {
    report_config_file_parse_error(err);
    return EXIT_FAILURE;
  }
  profile_store->save(profile);

  // Create the ViewModel - it owns the working copy of the profile
  auto view_model = std::make_shared<ViewModel>(std::move(profile));
  auto main_window = MainWindow{view_model, profile_store};
  main_window.show();

  return app.exec();
}

}  // namespace curves

auto main(int argc, char* argv[]) -> int { return curves::main(argc, argv); }
