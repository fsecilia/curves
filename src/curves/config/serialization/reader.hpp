// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Read side of the serializer.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <functional>
#include <string_view>
#include <utility>

namespace curves {

//! Format-agnostic, hierarchical reader for key-value data.
template <typename Adapter, typename ErrorReporter>
class Reader {
 public:
  Reader(Adapter adapter, ErrorReporter& error_reporter) noexcept
      : adapter_{std::move(adapter)}, error_reporter_{error_reporter} {}

  template <typename T>
  auto operator()(std::string_view key, T& value) -> void {
    adapter_.read_value(key, error_reporter_, value);
  }

  template <typename VisitSectionFunc>
  auto visit_section(std::string_view section_name,
                     VisitSectionFunc&& visit_section) -> void {
    if (auto section_adapter = adapter_.get_section(section_name)) {
      auto section_visitor = Reader{*section_adapter, error_reporter_};
      std::invoke(std::forward<VisitSectionFunc>(visit_section),
                  section_visitor);
    }
  }

  auto report_error(std::string_view message) const -> void {
    error_reporter_.report_error(message);
  }

 private:
  Adapter adapter_;
  ErrorReporter& error_reporter_;
};

}  // namespace curves
