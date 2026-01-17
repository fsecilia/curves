// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Data-driven config param.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/config/enum.hpp>
#include <algorithm>
#include <format>
#include <string_view>

namespace curves {

template <typename Visitor, typename Value>
concept CanReportError = requires(Visitor&& visitor, std::string message) {
  visitor.report_error(message);
};

template <typename Visitor, typename Value>
concept CanReportWarning = requires(Visitor&& visitor, std::string message) {
  visitor.report_warning(message);
};

template <typename T>
concept Mutable = !std::is_const_v<std::remove_reference_t<T>>;

template <typename Value>
class Param {
 public:
  Param(std::string_view name, Value value, Value min, Value max) noexcept
      : name_{name}, value_{value}, min_{min}, max_{max} {}

  auto name() const noexcept -> std::string_view { return name_; }
  auto value() const noexcept -> Value { return value_; }
  auto value(Value value) noexcept -> void { value_ = std::move(value); }
  auto min() const noexcept -> Value { return min_; }
  auto max() const noexcept -> Value { return max_; }

  auto reflect(this auto&& self, auto&& visitor) -> void {
    std::forward<decltype(visitor)>(visitor)(
        std::forward<decltype(self)>(self));
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& visitor = nullptr) -> void {
    if (value_ < min_ || value_ > max_) {
      const auto unclamped = value_;
      value_ = std::clamp(value_, min_, max_);

      if constexpr (CanReportWarning<decltype(visitor), Value>) {
        std::forward<Visitor>(visitor).report_warning(
            std::format("{} was out of range [{}, {}]: clamped from {} to {}",
                        name_, min_, max_, unclamped, value_));
      }
    }
  }

 private:
  std::string_view name_;
  Value value_;
  Value min_;
  Value max_;
};

template <Enumeration Enum>
class Param<Enum> {
 public:
  Param(std::string_view name, Enum value) noexcept
      : name_{name}, value_{value} {}

  auto name() const noexcept -> std::string_view { return name_; }
  auto value() const noexcept -> Enum { return value_; }
  auto value(Enum value) noexcept -> void { value_ = value; }

  auto reflect(this auto&& self, auto&& visitor) -> void {
    std::forward<decltype(visitor)>(visitor)(
        std::forward<decltype(self)>(self));
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& = nullptr) -> void {}

 private:
  std::string_view name_;
  Enum value_;
};

template <>
class Param<bool> {
 public:
  Param(std::string_view name, bool value) noexcept
      : name_{name}, value_{value} {}

  auto name() const noexcept -> std::string_view { return name_; }
  auto value() const noexcept -> bool { return value_; }
  auto value(bool value) noexcept -> void { value_ = value; }

  auto reflect(this auto&& self, auto&& visitor) -> void {
    std::forward<decltype(visitor)>(visitor)(
        std::forward<decltype(self)>(self));
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& = nullptr) -> void {}

 private:
  std::string_view name_;
  bool value_;
};

}  // namespace curves
