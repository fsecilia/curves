// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Writer api for a toml node.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace curves {

class TomlWriterAdapter {
 public:
  explicit TomlWriterAdapter(toml::table& table) : table_{table} {}

  template <typename T>
  auto write_value(std::string_view key, const T& value) -> void {
    table_.insert_or_assign(key, value);
  }

  auto create_section(std::string_view key) -> TomlWriterAdapter {
    auto& node = *table_.insert_or_assign(key, toml::table{}).first;
    toml::table* sub_table = node.second.as_table();
    return TomlWriterAdapter{*sub_table};
  }

 private:
  toml::table& table_;
};

}  // namespace curves
