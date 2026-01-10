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

enum CurveDefinition {
  kTransferGradient,
  kVelocityScale,
};

template <>
struct EnumReflection<CurveDefinition> {
  static constexpr auto map =
      sequential_name_map<CurveDefinition>("Gradient", "Scale");
};

template <typename CurveConfig>
struct CurveProfileEntry {
  std::string_view name;
  CurveConfig config;

  Param<CurveDefinition> definition{"Definition",
                                    CurveDefinition::kTransferGradient};

  CurveProfileEntry(std::string_view name, CurveConfig config = {}) noexcept
      : name{std::move(name)}, config{std::move(config)} {}

  auto reflect(this auto&& self, auto&& visitor) -> void {
    visitor.visit_section(self.name, [&](auto&& section_visitor) {
      self.config.reflect(section_visitor);
      self.definition.reflect(section_visitor);
    });
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    config.validate(visitor);
    definition.validate(visitor);
  }
};

}  // namespace curves
