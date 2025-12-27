// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel fixed point module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/fixed.h>
}  // extern "C"

#include <curves/lib.hpp>
#include <ostream>

namespace curves {

inline auto curves_fixed_from_double(double value,
                                     unsigned int frac_bits) noexcept -> s64 {
  const auto scaled_double =
      static_cast<double>(value) * static_cast<double>(1ll << frac_bits);
  const auto fixed = static_cast<s64>(scaled_double);
  return fixed;
}

inline auto curves_fixed_to_double(s64 raw, unsigned int frac_bits) noexcept
    -> double {
  return static_cast<double>(raw) / static_cast<double>(1ll << frac_bits);
}

struct Fixed {
  using Raw = int64_t;
  Raw raw;

  using WideValue = int128_t;

  inline static const int_t kFracBits = CURVES_FIXED_SHIFT;

  constexpr Fixed() = default;
  constexpr Fixed(std::signed_integral auto integer) noexcept
      : raw{static_cast<Raw>(integer) << kFracBits} {}
  explicit constexpr Fixed(std::floating_point auto real) noexcept
      : raw{static_cast<Raw>(std::round(real * (1LL << kFracBits)))} {}

  struct LiteralTag {};
  constexpr Fixed(Raw raw, LiteralTag) noexcept : raw{raw} {}
  static constexpr auto from_raw(Raw raw) noexcept -> Fixed {
    return Fixed{raw, LiteralTag{}};
  }

  constexpr auto to_int() const noexcept -> int_t { return raw >> kFracBits; }
  constexpr auto to_real() const noexcept -> real_t {
    return static_cast<real_t>(raw) / (1LL << kFracBits);
  }

  constexpr Fixed(const Fixed&) noexcept = default;
  constexpr auto operator=(const Fixed&) noexcept -> Fixed& = default;

  constexpr auto operator+=(const Fixed& src) noexcept -> Fixed& {
    raw += src.raw;
    return *this;
  }

  constexpr auto operator-=(const Fixed& src) noexcept -> Fixed& {
    raw -= src.raw;
    return *this;
  }

  constexpr auto operator*=(const Fixed& src) noexcept -> Fixed& {
    raw = (static_cast<WideValue>(raw) * src.raw) >> kFracBits;
    return *this;
  }

  constexpr auto operator/=(const Fixed& src) noexcept -> Fixed& {
    raw = (static_cast<WideValue>(raw) << kFracBits) / src.raw;
    return *this;
  }

  constexpr auto operator&=(const std::integral auto& src) noexcept -> Fixed& {
    raw &= src;
    return *this;
  }

  constexpr auto operator|=(const std::integral auto& src) noexcept -> Fixed& {
    raw |= src;
    return *this;
  }

  constexpr auto operator^=(const std::integral auto& src) noexcept -> Fixed& {
    raw ^= src;
    return *this;
  }

  constexpr auto operator>>=(const std::integral auto& src) noexcept -> Fixed& {
    raw >>= src;
    return *this;
  }

  constexpr auto operator<<=(const std::integral auto& src) noexcept -> Fixed& {
    raw <<= src;
    return *this;
  }

  friend constexpr auto operator+(Fixed left, const Fixed& right) noexcept
      -> Fixed {
    return left += right;
  }

  friend constexpr auto operator-(Fixed left, const Fixed& right) noexcept
      -> Fixed {
    return left -= right;
  }

  friend constexpr auto operator*(Fixed left, const Fixed& right) noexcept
      -> Fixed {
    return left *= right;
  }

  friend constexpr auto operator/(Fixed left, const Fixed& right) noexcept
      -> Fixed {
    return left /= right;
  }

  friend constexpr auto operator&(Fixed left,
                                  const std::integral auto& right) noexcept
      -> Fixed {
    return left &= right;
  }

  friend constexpr auto operator|(Fixed left,
                                  const std::integral auto& right) noexcept
      -> Fixed {
    return left |= right;
  }

  friend constexpr auto operator^(Fixed left,
                                  const std::integral auto& right) noexcept
      -> Fixed {
    return left ^= right;
  }

  friend constexpr auto operator<<(Fixed left,
                                   const std::integral auto& right) noexcept
      -> Fixed {
    return left <<= right;
  }

  friend constexpr auto operator>>(Fixed left,
                                   const std::integral auto& right) noexcept
      -> Fixed {
    return left >>= right;
  }

  friend auto operator<<(std::ostream& out, const Fixed& src) -> std::ostream& {
    return out << src.to_real();
  }

  constexpr auto operator==(const Fixed&) const noexcept -> bool = default;
  constexpr auto operator<=>(const Fixed&) const noexcept -> auto = default;

  constexpr auto mul_div(Fixed dividend, Fixed divisor) const noexcept
      -> Fixed {
    const auto numerator = static_cast<WideValue>(raw) * dividend.raw;
    return from_raw(static_cast<Raw>(numerator / divisor.raw));
  }

  constexpr auto fma(Fixed multiplier, Fixed addend) const noexcept -> Fixed {
    return from_raw(static_cast<Raw>(
        (static_cast<WideValue>(raw) * multiplier.raw + addend.raw) >>
        kFracBits));
  }

  constexpr auto reciprocal() const noexcept -> Fixed {
    return from_raw(
        static_cast<Raw>((static_cast<WideValue>(1) << (2 * kFracBits)) / raw));
  }

  constexpr auto floor() const noexcept -> Fixed {
    return from_raw(raw & ~((1LL << kFracBits) - 1));
  }
};

}  // namespace curves
