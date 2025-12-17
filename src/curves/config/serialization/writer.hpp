// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Write side of the serializer.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <functional>
#include <string_view>
#include <utility>

namespace curves {

//! Format-agnostic, hierarchical writer for key-value data.
template <typename Adapter>
class Writer {
 public:
  explicit Writer(Adapter adapter) noexcept : adapter_{std::move(adapter)} {}

  template <typename T>
  auto operator()(std::string_view key, const T& value) -> void {
    adapter_.write_value(key, value);
  }

  template <typename VisitSectionFunc>
  auto visit_section(std::string_view section_name,
                     VisitSectionFunc&& visit_section) -> void {
    auto section_adapter = adapter_.create_section(section_name);
    auto section_visitor = Writer{std::move(section_adapter)};
    std::invoke(std::forward<VisitSectionFunc>(visit_section), section_visitor);
  }

 private:
  Adapter adapter_;
};

}  // namespace curves
