// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <complex>
#include <type_traits>

namespace crv {

//
// is_complex
//

namespace detail {

template <typename value_t> struct is_complex_f : std::bool_constant<false>
{};

template <typename value_t> struct is_complex_f<std::complex<value_t>> : std::bool_constant<true>
{};

template <typename value_t> inline constexpr auto is_complex_v = is_complex_f<value_t>::value;

} // namespace detail

template <typename value_t>
concept is_complex = detail::is_complex_v<value_t>;

//
// real_type_t
//

namespace detail {

template <typename value_t> struct real_type_f
{
    using type = value_t;
};

template <typename value_t> struct real_type_f<std::complex<value_t>>
{
    using type = value_t;
};

} // namespace detail

/// extracts real number type from std::complex, passthrough for other types
///
/// This complements std::real() at the type level.
template <typename value_t> using real_type_t = typename detail::real_type_f<value_t>::type;

} // namespace crv
