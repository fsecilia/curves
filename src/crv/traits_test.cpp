// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "traits.hpp"
#include <crv/test/test.hpp>
#include <concepts>

namespace crv {
namespace {

namespace copy_cv {

struct src_t
{};

struct dst_t
{};

static_assert(std::same_as<copy_cv_t<dst_t, src_t>, dst_t>);
static_assert(std::same_as<copy_cv_t<dst_t, src_t const>, dst_t const>);
static_assert(std::same_as<copy_cv_t<dst_t, src_t volatile>, dst_t volatile>);
static_assert(std::same_as<copy_cv_t<dst_t, src_t const volatile>, dst_t const volatile>);

} // namespace copy_cv

} // namespace
} // namespace crv
