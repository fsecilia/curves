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
  // This isn't really a parameter that needs clamping, but we *really* need
  // to have a version number in a formated file from day 0, and this is
  // expedient.
  // TODO: refactor clamping into a decorator.
  Param<int_t> version{"version", 1, 0, std::numeric_limits<int_t>::max()};

  Param<int_t> dpi{"dpi", 0, 0, 256000};
  Param<double> sensitivity{"sensitivity", 1.0, 1.0e-3, 1.0e3};
  Param<CurveType> curve_type{"curve", CurveType::kSynchronous};
  Param<double> anisotropy{"yx_scaling", 1.0, 1.0e-3, 1.0e3};
  Param<double> rotation{"rotation", 1.0, 1.0e-3, 1.0e3};

  Param<bool> speed_filter_enabled{"speed_filter_enabled", true};
  Param<double> speed_filter_halflife{"speed_filter_halflife", 2.0, 1.0e-3,
                                      1.0e3};
  Param<bool> scale_filter_enabled{"scale_filter_enabled", true};
  Param<double> scale_filter_halflife{"scale_filter_halflife", 2.0, 1.0e-3,
                                      1.0e3};
  Param<bool> output_filter_enabled{"output_filter_enabled", true};
  Param<double> output_filter_halflife{"output_filter_halflife", 2.0, 1.0e-3,
                                       1.0e3};

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
    self.version.reflect(visitor);
    self.dpi.reflect(visitor);
    self.sensitivity.reflect(visitor);
    self.curve_type.reflect(visitor);
    self.anisotropy.reflect(visitor);
    self.rotation.reflect(visitor);

    self.speed_filter_enabled.reflect(visitor);
    self.speed_filter_halflife.reflect(visitor);
    self.scale_filter_enabled.reflect(visitor);
    self.scale_filter_halflife.reflect(visitor);
    self.output_filter_enabled.reflect(visitor);
    self.output_filter_halflife.reflect(visitor);

    self.curve_profile_entries.reflect(visitor);
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    version.validate(visitor);
    dpi.validate(visitor);
    sensitivity.validate(visitor);
    curve_type.validate(visitor);
    anisotropy.validate(visitor);
    rotation.validate(visitor);

    speed_filter_enabled.validate(visitor);
    speed_filter_halflife.validate(visitor);
    scale_filter_enabled.validate(visitor);
    scale_filter_halflife.validate(visitor);
    output_filter_enabled.validate(visitor);
    output_filter_halflife.validate(visitor);

    curve_profile_entries.validate(visitor);
  }
};

}  // namespace curves
