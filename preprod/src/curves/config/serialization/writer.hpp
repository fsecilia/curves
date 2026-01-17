// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Write side of the serializer.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/param.hpp>
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
  auto operator()(const Param<T>& param) -> void {
    adapter_.write_value(param.name(), param.value());
  }

  template <Enumeration Enum>
  auto operator()(const Param<Enum>& param) -> void {
    adapter_.write_value(param.name(), to_string(param.value()));
  }

  template <typename VisitSectionFunc>
  auto visit_section(std::string_view section_name,
                     VisitSectionFunc&& visit_section) -> void {
    std::invoke(std::forward<VisitSectionFunc>(visit_section),
                Writer{adapter_.create_section(section_name)});
  }

 private:
  Adapter adapter_;
};

}  // namespace curves
