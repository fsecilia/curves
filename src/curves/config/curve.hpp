// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Data-driven curve config.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/enum.hpp>
#include <curves/config/param.hpp>
#include <string_view>

namespace curves {

enum CurveInterpretation {
  kGain,
  kSensitivity,
};

template <>
struct EnumReflection<CurveInterpretation> {
  static constexpr auto map =
      sequential_name_map<CurveInterpretation>("Gain", "Sensitivity");
};

template <typename CurveConfig>
struct CurveProfileEntry {
  std::string_view name;
  CurveConfig config;

  Param<CurveInterpretation> interpretation{"Interpretation",
                                            CurveInterpretation::kGain};

  CurveProfileEntry(std::string_view name, CurveConfig config = {}) noexcept
      : name{std::move(name)}, config{std::move(config)} {}

  auto reflect(this auto&& self, auto&& visitor) -> void {
    visitor.visit_section(self.name, [&](auto&& section_visitor) {
      self.config.reflect(section_visitor);
      self.interpretation.reflect(section_visitor);
    });
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    config.validate(visitor);
    interpretation.validate(visitor);
  }
};

}  // namespace curves
