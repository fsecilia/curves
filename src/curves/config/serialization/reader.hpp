// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Read side of the serializer.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/param.hpp>
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
  auto operator()(Param<T>& param) -> void {
    T value = param.value();
    adapter_.read_value(param.name(), error_reporter_, value);
    param.value(value);
  }

  template <Enumeration Enum>
  auto operator()(Param<Enum>& param) -> void {
    auto as_string = to_string(param.value());

    adapter_.read_value(param.name(), error_reporter_, as_string);

    if (auto val = from_string<Enum>(as_string)) {
      param.value(*val);
    } else if constexpr (CanReportError<decltype(error_reporter_), Enum>) {
      error_reporter_.report_error(
          std::format("Invalid enum value: {}", as_string));
    }
  }

  template <typename VisitSectionFunc>
  auto visit_section(std::string_view section_name,
                     VisitSectionFunc&& visit_section) -> void {
    if (auto section_adapter = adapter_.get_section(section_name)) {
      std::invoke(std::forward<VisitSectionFunc>(visit_section),
                  Reader{std::move(*section_adapter), error_reporter_});
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
