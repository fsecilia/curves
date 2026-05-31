// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "complex_traits.hpp"
#include <crv/test/test.hpp>
#include <concepts>

namespace crv {
namespace {

struct arbitrary_t
{};

//
// is_complex
//

static_assert(!is_complex<arbitrary_t>);
static_assert(!is_complex<int_t>);
static_assert(!is_complex<float32_t>);
static_assert(!is_complex<float64_t>);

static_assert(is_complex<std::complex<float32_t>>);
static_assert(is_complex<std::complex<float64_t>>);

//
// real_type_t
//

static_assert(std::same_as<real_type_t<arbitrary_t>, arbitrary_t>);
static_assert(std::same_as<real_type_t<int_t>, int_t>);
static_assert(std::same_as<real_type_t<float32_t>, float32_t>);
static_assert(std::same_as<real_type_t<float64_t>, float64_t>);

static_assert(std::same_as<real_type_t<std::complex<float32_t>>, float32_t>);
static_assert(std::same_as<real_type_t<std::complex<float64_t>>, float64_t>);

} // namespace
} // namespace crv
