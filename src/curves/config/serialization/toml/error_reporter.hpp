// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Error reporting for a toml node.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace curves {

class ErrorReporter {
 public:
  //! \brief Records most recently visited source region.
  auto location(const toml::source_region& src) -> void { location_ = src; }

  /*!
    \brief Throws a parse error using the recorded location with given message.
  */
  [[noreturn]] auto emit_error(std::string_view message) const -> void {
    throw toml::parse_error{message.data(), location_};
  }

 private:
  toml::source_region location_{};
};

}  // namespace curves
