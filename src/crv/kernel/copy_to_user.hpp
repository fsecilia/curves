// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <cstddef>
#include <span>

extern "C" {

unsigned long crv_copy_to_user(void* dst, void const* src, unsigned long length);

} // extern "C" {

namespace crv::kernel {

/// wraps kernel c copy_to_user in a freestanding c++ functor
struct copy_to_user_t
{
    [[nodiscard]] auto operator()(void* dst, std::span<std::byte const> src) -> std::size_t
    {
        return int_cast<std::size_t>(crv_copy_to_user(dst, src.data(), int_cast<unsigned long>(src.size())));
    }
};

struct copy_to_user_factory_t
{
    using product_t = copy_to_user_t;

    constexpr auto operator()() const noexcept -> product_t { return product_t{}; }
};

} // namespace crv::kernel
