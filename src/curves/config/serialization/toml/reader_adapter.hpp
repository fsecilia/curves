// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Reader api for a toml node.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <optional>
#include <string_view>
#include <toml++/toml.hpp>

namespace curves {

class TomlReaderAdapter {
 public:
  explicit TomlReaderAdapter(const toml::table& table) : table_{table} {}

  template <typename T>
  auto read_value(std::string_view key, auto& error_reporter, T& dest) -> void {
    if (auto* node = table_.get(key)) {
      error_reporter.location(node->source());
      if (auto val = node->value<T>()) {
        dest = std::move(*val);
      }
    }
  }

  auto get_section(std::string_view key) const
      -> std::optional<TomlReaderAdapter> {
    if (auto* node = table_.get(key)) {
      if (auto* sub_table = node->as_table()) {
        return TomlReaderAdapter{*sub_table};
      }
    }
    return std::nullopt;
  }

 private:
  const toml::table& table_;
};

}  // namespace curves
