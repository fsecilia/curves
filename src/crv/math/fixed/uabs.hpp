// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/integer.hpp>

namespace crv {

/// converts to unsigned equivalent using abs magnitude
///
/// This function is |x|, calculated in the unsigned domain, so the most-negative value is still well-defined. Standard
/// abs, taken on the most-negative value for a signed type, is unrepresentable in the original signed type.
///
/// \returns magnitude of input in the unsigned counterpart of value_t
template <is_fixed input_t>
    requires(crv::is_signed_v<typename input_t::value_t>)
constexpr auto uabs(input_t input) noexcept -> fixed_t<make_unsigned_t<typename input_t::value_t>, input_t::frac_bits>
{
    using unsigned_t = make_unsigned_t<typename input_t::value_t>;
    using magnitude_t = fixed_t<unsigned_t, input_t::frac_bits>;

    auto const unsigned_value = static_cast<unsigned_t>(input.value);
    return magnitude_t::literal(input.value < 0 ? unsigned_t{0} - unsigned_value : unsigned_value);
}

// unsigned specialization returns input, unmodified
template <is_fixed input_t>
    requires(!crv::is_signed_v<typename input_t::value_t>)
constexpr auto uabs(input_t input) noexcept -> input_t
{
    return input;
}

} // namespace crv
