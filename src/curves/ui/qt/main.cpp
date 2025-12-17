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
#include <QMessageBox>
#include <QStandardPaths>
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

auto get_config_dir_path() -> std::string {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
      .toStdString();
}

auto create_default_profile() -> curves::Profile {
  auto profile = curves::Profile{};
  profile.curves.emplace_back(curves::modes::synchronous::Config::create());
  return profile;
}

class TomlReaderVisitor {
 public:
  explicit TomlReaderVisitor(const toml::table& table) : table_{table} {}

  template <typename T>
  auto operator()(std::string_view key, T& value) -> void {
    if (auto node = table_.get(key)) {
      last_source_ = node->source();
      if (auto val = node->value<T>()) {
        value = *val;
      }
    }
  }

  // Handle the string_view proxy for Enums.
  auto operator()(std::string_view key, std::string_view& value) -> void {
    if (auto node = table_.get(key)) {
      last_source_ = node->source();
      if (auto val = node->value<std::string_view>()) {
        value = *val;
      }
    }
  }

  auto operator()(std::string_view key, std::string& value) -> void {
    if (auto node = table_.get(key)) {
      last_source_ = node->source();
      if (auto val = node->value<std::string>()) {
        value = std::move(*val);
      }
    }
  }

  template <typename Func>
  auto visit_section(std::string_view section_name, Func&& func) -> void {
    // Try to find table.
    if (auto node = table_.get(section_name)) {
      if (auto* sub_table = node->as_table()) {
        // Create a temporary visitor for this sub-table.
        TomlReaderVisitor sub_visitor{*sub_table};
        // Execute callback with narrowed visitor.
        std::invoke(std::forward<Func>(func), sub_visitor);
      }
    }
  }

  auto report_error(std::string_view message) {
    throw toml::parse_error{message.data(), last_source_};
  }

 private:
  const toml::table& table_;
  toml::source_region last_source_{};
};

class TomlWriterVisitor {
 public:
  explicit TomlWriterVisitor(toml::table& table) : table_{table} {}

  template <typename T>
  auto operator()(std::string_view key, const T& value) -> void {
    table_.insert_or_assign(key, value);
  }

  template <typename Func>
  auto visit_section(std::string_view section_name, Func&& func) -> void {
    // Get or Create the sub-table
    auto& node = *table_.insert_or_assign(section_name, toml::table{}).first;
    toml::table* sub_table = node.second.as_table();

    // Recurse
    TomlWriterVisitor sub_visitor{*sub_table};
    std::invoke(std::forward<Func>(func), sub_visitor);
  }

 private:
  toml::table& table_;
};

auto save_config(const curves::Profile& config,
                 const std::filesystem::path& path) -> void {
  toml::table root;

  TomlWriterVisitor visitor{root};

  config.reflect(visitor);

  std::ofstream file(path);
  file << root;
}

auto load_config(curves::Profile& config, const std::filesystem::path& path)
    -> void {
  /*
    This is technically toctou, but we'll refactor and handle file io ourselves
    once we get a curve hooked up.
  */
  if (!std::filesystem::exists(path)) return;

  auto root = toml::parse_file(path.string());
  TomlReaderVisitor visitor{root};

  // "Reflect" the visitor into the config
  config.reflect(visitor);

  // Always validate after loading external data!
  config.validate();
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
  std::filesystem::create_directories(config_dir_path);

  const auto config_file_path = config_dir_path + "/config.toml";
  auto profile = create_default_profile();

  try {
    load_config(profile, config_file_path);
  } catch (const toml::parse_error& err) {
    report_config_file_parse_error(err);
    return EXIT_FAILURE;
  }

  save_config(profile, config_file_path);

  auto main_window = MainWindow{};

  main_window.prepopulateCurveParameterWidgets(10);

  auto sensitivity = curves::modes::synchronous::Curve{
      0.433012701892L, 17.3205080757L, 5.33L, 28.3L, 0.5L};
  main_window.setSpline(std::make_shared<curves_spline>(
      curves::spline::create_spline(curves::TransferFunction{sensitivity})));

  main_window.show();

  return app.exec();
}
