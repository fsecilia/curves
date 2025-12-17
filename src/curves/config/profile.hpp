// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Data-driven profile.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/config/param.hpp>
#include <string_view>
#include <vector>

namespace curves {

struct Profile {
  Param<int_t> dpi{"DPI", 0, 0, 256000};
  Param<double> sensitivity{"Sensitivity", 1.0, 1.0e-3, 1.0e3};

  std::vector<Curve> curves;

  auto reflect(this auto&& self, auto&& visitor) -> void {
    self.dpi.reflect(visitor);
    self.sensitivity.reflect(visitor);

    for (auto&& curve : self.curves) {
      visitor.visit_section(curve.name, [&](auto&& section_visitor) {
        curve.reflect(section_visitor);
      });
    }
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    dpi.validate(visitor);
    sensitivity.validate(visitor);

    for (auto& curve : curves) curve.validate(visitor);
  }
};

}  // namespace curves
