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
#include <curves/math/curves/synchronous.hpp>
#include <cassert>
#include <functional>
#include <string_view>

namespace curves {

enum CurveType {
  kSynchronous,
};

template <>
struct EnumReflection<CurveType> {
  static constexpr auto map = sequential_name_map<CurveType>("Synchronous");
};

struct Profile {
  // This isn't really a parameter that needs clamping, but we *really* need
  // to have a version number in a formated file from day 0, and this is
  // expedient.
  // TODO: refactor clamping into a decorator.
  Param<int_t> version{"Version", 1, 0, std::numeric_limits<int_t>::max()};

  Param<CurveType> curve_type{"Curve", CurveType::kSynchronous};
  Param<int_t> dpi{"Mouse DPI", 0, 0, 256000};
  Param<double> sensitivity{"Sensitivity", 1.0, 1.0e-3, 1.0e3};
  Param<double> anisotropy{"Y/X Scaling", 1.0, 1.0e-3, 1.0e3};
  Param<double> rotation{"Rotation", 0.0, -360.0, 360.0};

  Param<bool> filter_speed{"Filter Speed", true};
  Param<double> speed_filter_halflife{"Speed Filter Halflife", 2.0, 1.0e-3,
                                      1.0e3};
  Param<bool> filter_scale{"Filter Scale", true};
  Param<double> scale_filter_halflife{"Scale Filter Halflife", 2.0, 1.0e-3,
                                      1.0e3};
  Param<bool> filter_output{"Filter Output", true};
  Param<double> output_filter_halflife{"Output Filter Halflife", 2.0, 1.0e-3,
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

    self.filter_speed.reflect(visitor);
    self.speed_filter_halflife.reflect(visitor);
    self.filter_scale.reflect(visitor);
    self.scale_filter_halflife.reflect(visitor);
    self.filter_output.reflect(visitor);
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

    filter_speed.validate(visitor);
    speed_filter_halflife.validate(visitor);
    filter_scale.validate(visitor);
    scale_filter_halflife.validate(visitor);
    filter_output.validate(visitor);
    output_filter_halflife.validate(visitor);

    curve_profile_entries.validate(visitor);
  }
};

}  // namespace curves
