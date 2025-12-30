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

struct Fixed {
  using Value = int64_t;
  Value raw;

  using WideValue = int128_t;

  inline static const int_t kFracBits = CURVES_FIXED_SHIFT;
  inline static const int_t kOne = 1LL << kFracBits;

  constexpr Fixed() = default;
  constexpr Fixed(std::signed_integral auto integer) noexcept
      : raw{static_cast<Value>(integer) << kFracBits} {}
  explicit constexpr Fixed(std::floating_point auto real) noexcept
      : raw{static_cast<Value>(std::round(real * kOne))} {}

  struct LiteralTag {};
  constexpr Fixed(Value raw, LiteralTag) noexcept : raw{raw} {}
  static constexpr auto from_raw(Value raw) noexcept -> Fixed {
    return Fixed{raw, LiteralTag{}};
  }

  constexpr auto to_int() const noexcept -> int_t { return raw >> kFracBits; }
  constexpr auto to_real() const noexcept -> real_t {
    return static_cast<real_t>(raw) / kOne;
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
    raw = curves_fixed_multiply_round(raw, src.raw);
    return *this;
  }

  constexpr auto operator/=(const Fixed& src) noexcept -> Fixed& {
    raw = curves_fixed_divide_round(raw, src.raw);
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

  constexpr auto fma(Fixed multiplicand, Fixed addend) const noexcept -> Fixed {
    return from_raw(curves_fixed_fma_round(raw, multiplicand.raw, addend.raw));
  }

  constexpr auto reciprocal() const noexcept -> Fixed {
    return from_raw(curves_fixed_divide_round(kOne, raw));
  }

  constexpr auto floor() const noexcept -> Fixed {
    return from_raw(raw & ~(kOne - 1));
  }
};

inline auto fixed_to_real(s64 raw) noexcept -> real_t {
  return Fixed::from_raw(raw).to_real();
}

inline auto real_to_fixed(real_t value) noexcept -> s64 {
  return Fixed{value}.raw;
}

}  // namespace curves
