// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <complex>

namespace crv {

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
