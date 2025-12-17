// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Config app qt entry point.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "main_window/main_window.hpp"
#include <curves/config/profile.hpp>
#include <curves/config/serialization/reader.hpp>
#include <curves/config/serialization/toml/error_reporter.hpp>
#include <curves/config/serialization/toml/reader_adapter.hpp>
#include <curves/config/serialization/toml/writer_adapter.hpp>
#include <curves/config/serialization/writer.hpp>
#include <curves/math/spline.hpp>
#include <curves/math/transfer_function.hpp>
#include <curves/modes/synchronous.hpp>
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <filesystem>
#include <fstream>

namespace curves {

auto get_config_dir_path() -> std::filesystem::path {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
      .toStdString();
}

auto create_default_profile() -> Profile {
  auto profile = Profile{};
  profile.curves.emplace_back(modes::synchronous::Config::create());
  return profile;
}

auto save_profile(const Profile& profile,
                  const std::filesystem::path& config_dir_path,
                  const std::filesystem::path& config_file_path) -> void {
  toml::table root;

  using TomlWriterVisitor = Writer<TomlWriterAdapter>;
  auto visitor = TomlWriterVisitor{TomlWriterAdapter{root}};

  profile.reflect(visitor);

  std::filesystem::create_directories(config_dir_path);
  std::ofstream file(config_file_path);
  file << root;
}

auto load_or_create_profile(const std::filesystem::path& config_file_path)
    -> Profile {
  auto result = create_default_profile();

  /*
    This is technically toctou, but we'll refactor and handle file io ourselves
    once we get a curve hooked up.
  */
  if (!std::filesystem::exists(config_file_path)) return result;

  auto root = toml::parse_file(config_file_path.string());
  auto error_reporter = ErrorReporter{};

  using TomlReaderVisitor = Reader<TomlReaderAdapter, ErrorReporter>;
  auto visitor = TomlReaderVisitor{TomlReaderAdapter{root}, error_reporter};
  result.reflect(visitor);

  // Validate after loading external data.
  result.validate();

  return result;
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

  const auto config_dir_path = get_config_dir_path();

  const auto config_file_path = config_dir_path / "config.toml";

  auto profile = Profile{};

  try {
    profile = load_or_create_profile(config_file_path);
  } catch (const toml::parse_error& err) {
    report_config_file_parse_error(err);
    return EXIT_FAILURE;
  }

  save_profile(profile, config_dir_path, config_file_path);

  auto main_window = MainWindow{};

  main_window.prepopulateCurveParameterWidgets(10);

  auto sensitivity = modes::synchronous::Curve{0.433012701892L, 17.3205080757L,
                                               5.33L, 28.3L, 0.5L};
  main_window.setSpline(std::make_shared<curves_spline>(
      spline::create_spline(TransferFunction{sensitivity})));

  main_window.show();

  return app.exec();
}

}  // namespace curves

auto main(int argc, char* argv[]) -> int { return curves::main(argc, argv); }
