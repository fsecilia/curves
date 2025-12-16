// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel fixed point module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/fixed.h>
#include <curves/driver/spline.h>
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

inline auto curves_fixed_to_double(s64 value, unsigned int frac_bits) noexcept
    -> double {
  return static_cast<double>(value) / static_cast<double>(1ll << frac_bits);
}

struct Fixed {
  using Value = int64_t;
  Value value;

  using WideValue = int128_t;

  /*
    The frac bits used by spline is currently the source of truth. This is
    temporary until we sort out the UAPI. Since the only fixed point we do
    on the usermode side is generating the spline, this may hold.
  */
  inline static const int_t frac_bits = SPLINE_FRAC_BITS;

  constexpr Fixed(std::signed_integral auto integer) noexcept
      : value{static_cast<Value>(integer) << frac_bits} {}
  explicit constexpr Fixed(std::floating_point auto real) noexcept
      : value{static_cast<Value>(std::round(real * (1LL << frac_bits)))} {}

  struct LiteralTag {};
  constexpr Fixed(Value value, LiteralTag) noexcept : value{value} {}
  static constexpr auto literal(Value value) noexcept -> Fixed {
    return Fixed{value, LiteralTag{}};
  }

  constexpr auto to_int() const noexcept -> int_t { return value >> frac_bits; }
  constexpr auto to_real() const noexcept -> real_t {
    return static_cast<real_t>(value) / (1LL << frac_bits);
  }

  constexpr Fixed(const Fixed&) noexcept = default;
  constexpr auto operator=(const Fixed&) noexcept -> Fixed& = default;

  constexpr auto operator+=(const Fixed& src) noexcept -> Fixed& {
    value += src.value;
    return *this;
  }

  constexpr auto operator-=(const Fixed& src) noexcept -> Fixed& {
    value -= src.value;
    return *this;
  }

  constexpr auto operator*=(const Fixed& src) noexcept -> Fixed& {
    value = (static_cast<WideValue>(value) * src.value) >> frac_bits;
    return *this;
  }

  constexpr auto operator/=(const Fixed& src) noexcept -> Fixed& {
    value = (static_cast<WideValue>(value) << frac_bits) / src.value;
    return *this;
  }

  constexpr auto operator&=(const std::integral auto& src) noexcept -> Fixed& {
    value &= src;
    return *this;
  }

  constexpr auto operator|=(const std::integral auto& src) noexcept -> Fixed& {
    value |= src;
    return *this;
  }

  constexpr auto operator^=(const std::integral auto& src) noexcept -> Fixed& {
    value ^= src;
    return *this;
  }

  constexpr auto operator>>=(const std::integral auto& src) noexcept -> Fixed& {
    value >>= src;
    return *this;
  }

  constexpr auto operator<<=(const std::integral auto& src) noexcept -> Fixed& {
    value <<= src;
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
    const auto numerator = static_cast<WideValue>(value) * dividend.value;
    return literal(static_cast<Value>(numerator / divisor.value));
  }

  constexpr auto fma(Fixed multiplier, Fixed addend) const noexcept -> Fixed {
    return literal(static_cast<Value>(
        (static_cast<WideValue>(value) * multiplier.value + addend.value) >>
        frac_bits));
  }

  constexpr auto reciprocal() const noexcept -> Fixed {
    return literal(static_cast<Value>(
        (static_cast<WideValue>(1) << (2 * frac_bits)) / value));
  }

  constexpr auto floor() const noexcept -> Fixed {
    return literal(value & ~((1LL << frac_bits) - 1));
  }
};

}  // namespace curves
