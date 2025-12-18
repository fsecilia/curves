// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Data-driven profile.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/config/enum.hpp>
#include <curves/config/param.hpp>
#include <curves/curves/synchronous.hpp>
#include <functional>
#include <string_view>

namespace curves {

enum CurveType {
  kSynchronous,
};

template <>
struct EnumReflection<CurveType> {
  static constexpr auto map = sequential_name_map<CurveType>("synchronous");
};

struct Profile {
  Param<int_t> dpi{"DPI", 0, 0, 256000};
  Param<double> sensitivity{"Sensitivity", 1.0, 1.0e-3, 1.0e3};
  Param<CurveType> curve_type{"Curve", CurveType::kSynchronous};

  struct CurveProfileEntries {
    CurveProfileEntry<SynchronousCurveConfig> synchronous{"synchronous"};

    auto reflect(this auto&& self, auto&& visitor) -> void {
      self.synchronous.reflect(visitor);
    }

    template <typename Visitor = std::nullptr_t>
    auto validate(Visitor&& visitor = nullptr) -> void {
      synchronous.validate(visitor);
    }

    // Visits CurveProfileEntry specific to given curve.
    auto visit_config(this auto&& self, CurveType curve, auto&& visitor)
        -> void {
      switch (curve) {
        case CurveType::kSynchronous:
          std::invoke(std::forward<decltype(visitor)>(visitor),
                      std::forward<decltype(self)>(self).synchronous);
          break;
      }
    }
  };
  CurveProfileEntries curve_profile_entries;

  auto reflect(this auto&& self, auto&& visitor) -> void {
    self.dpi.reflect(visitor);
    self.sensitivity.reflect(visitor);
    self.curve_type.reflect(visitor);
    self.curve_profile_entries.reflect(visitor);
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    dpi.validate(visitor);
    sensitivity.validate(visitor);
    curve_type.validate(visitor);
    curve_profile_entries.validate(visitor);
  }
};

}  // namespace curves
