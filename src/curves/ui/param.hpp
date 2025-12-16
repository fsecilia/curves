// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel spline module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <algorithm>
#include <string_view>

namespace curves {

template <typename Visitor, typename Value>
concept CanReportClamp =
    requires(Visitor&& visitor, std::string_view name, Value value) {
      visitor.on_clamp(name, value, value, value, value);
    };

template <typename Value>
class Param {
 public:
  Param(std::string_view name, Value value, Value min, Value max) noexcept
      : name_{name}, value_{value}, min_{min}, max_{max} {}

  auto name() const noexcept -> std::string_view { return name_; }
  auto value() const noexcept -> Value { return value_; }
  auto min() const noexcept -> Value { return min_; }
  auto max() const noexcept -> Value { return max_; }

  auto reflect(this auto&& self, auto&& visitor) -> void {
    visitor(self.name_, self.value_);
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    if (value_ < min_ || value_ > max_) {
      const auto unclamped = value_;
      value_ = std::clamp(value_, min_, max_);

      if constexpr (CanReportClamp<Visitor, Value>) {
        visitor.on_clamp(name_, unclamped, min_, max_, value_);
      }
    }
  }

 private:
  std::string_view name_;
  Value value_;
  Value min_;
  Value max_;
};

}  // namespace curves
