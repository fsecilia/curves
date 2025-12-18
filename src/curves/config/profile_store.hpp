// SPDX-License-Identifier: MIT
/*!
  \file
  \brief TOML profile store.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/config/profile.hpp>
#include <curves/config/serialization/reader.hpp>
#include <curves/config/serialization/toml/error_reporter.hpp>
#include <curves/config/serialization/toml/reader_adapter.hpp>
#include <curves/config/serialization/toml/writer_adapter.hpp>
#include <curves/config/serialization/writer.hpp>
#include <filesystem>
#include <fstream>

namespace curves {

class ProfileStore {
 public:
  explicit ProfileStore(std::filesystem::path path) noexcept
      : path_{std::move(path)} {}

  [[nodiscard]] auto find_or_create() -> Profile {
    auto result = Profile{};

    auto root = toml::parse_file(path_.string());
    auto error_reporter = ErrorReporter{};

    using TomlReaderVisitor = Reader<TomlReaderAdapter, ErrorReporter>;
    auto visitor = TomlReaderVisitor{TomlReaderAdapter{root}, error_reporter};
    result.reflect(visitor);
    result.validate();

    return result;
  }

  auto save(const Profile& profile) -> void {
    toml::table root;

    using TomlWriterVisitor = Writer<TomlWriterAdapter>;
    auto visitor = TomlWriterVisitor{TomlWriterAdapter{root}};

    profile.reflect(visitor);

    std::filesystem::create_directories(path_.parent_path());
    std::ofstream file(path_);
    file << root;
  }

 private:
  std::filesystem::path path_;
};

}  // namespace curves
