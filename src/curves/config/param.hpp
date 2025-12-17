// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Data-driven config param.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
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
concept Enum = std::is_enum_v<T>;

template <typename T>
concept Mutable = !std::is_const_v<std::remove_reference_t<T>>;

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

      if constexpr (CanReportWarning<decltype(visitor), Value>) {
        visitor.report_warning(
            std::format("{} was out of range [{}, {}): clamped from {} to {}",
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

template <typename>
struct EnumTraits;

template <Enum Value>
class Param<Value> {
 public:
  Param(std::string_view name, Value value) noexcept
      : name_{name}, value_{value} {}

  auto name() const noexcept -> std::string_view { return name_; }
  auto value() const noexcept -> Value { return value_; }

  auto reflect(this auto&& self, auto&& visitor) -> void {
    // Proxy the enum into a string_view with the same constness as self.
    using Proxy = std::conditional_t<Mutable<decltype(self)>, std::string_view,
                                     const std::string_view>;
    Proxy proxy{EnumTraits<Value>::to_string(self.value_)};

    // Visit string instead of enum value.
    visitor(self.name_, proxy);

    if constexpr (Mutable<decltype(self)>) {
      // Convert contents of proxy back to enum.
      const auto value = EnumTraits<Value>::from_string(proxy);

      if (value) {
        // Conversion was successful; proxy matches an enum string.
        self.value_ = *value;
      } else {
        /*
          Conversion was unsuccessful.
          Report unrecognized enum string if visitor supports it.
        */
        if constexpr (CanReportError<decltype(visitor), Value>) {
          visitor.report_error(std::format("Invalid enum value: {}", proxy));
        }
      }
    }
  }

  template <typename Visitor = std::nullptr_t>
  auto validate(Visitor&& = nullptr) -> void {}

 private:
  std::string_view name_;
  Value value_;
};

}  // namespace curves
