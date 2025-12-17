// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Data-driven curve config.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/param.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace curves {

struct Curve {
  std::string_view name;

  enum Interpretation {
    kGain,
    kSensitivity,
  };
  Param<Interpretation> interpretation;

  std::vector<Param<double>> double_params;

  auto reflect(this auto&& self, auto&& visitor) -> void {
    self.interpretation.reflect(visitor);
    for (auto&& double_param : self.double_params) {
      double_param.reflect(visitor);
    }
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    interpretation.validate(visitor);
    for (auto& double_param : double_params) double_param.validate(visitor);
  }
};

template <>
struct EnumTraits<Curve::Interpretation> {
  static constexpr std::string_view names[] = {
      "gain",
      "sensitivity",
  };

  static constexpr std::string_view to_string(Curve::Interpretation value) {
    const auto index = static_cast<size_t>(value);
    if (index < std::size(names)) {
      return names[index];
    }
    return "unknown";
  }

  static constexpr std::optional<Curve::Interpretation> from_string(
      std::string_view s) {
    for (size_t i = 0; i < std::size(names); ++i) {
      if (names[i] == s) {
        return static_cast<Curve::Interpretation>(i);
      }
    }
    return std::nullopt;
  }
};

}  // namespace curves
