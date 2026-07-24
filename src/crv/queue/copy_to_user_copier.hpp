// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <cstddef>
#include <span>
#include <utility>

namespace crv {

/// adapts a copy_to_user()-style residual copy to the spsc byte-copier protocol
///
/// The injected residual copy receives a user destination and contiguous source span, and returns the number of bytes
/// not copied.
///
/// A nonzero residual is converted to a short copied-byte count and records the injected fault error. The error remains
/// available after spsc_t::read() returns.
///
/// One instance represents one spsc read operation. After a short copy, it must not be invoked again.
template <typename residual_copy_t> class copy_to_user_copier_t
{
public:
    copy_to_user_copier_t(void* dst, residual_copy_t residual_copy, int fault_error) noexcept
        : dst_{static_cast<std::byte*>(dst)}, residual_copy_{std::move(residual_copy)}, fault_error_{fault_error}
    {
        assert(fault_error_ < 0);
    }

    [[nodiscard]] auto operator()(std::size_t dst_offset, std::span<std::byte const> src) noexcept -> std::size_t
    {
        // spsc_t must stop invoking the copier after a short copy.
        assert(error_ == 0);

        auto const uncopied = residual_copy_(dst_ + dst_offset, src);
        assert(uncopied <= src.size());
        if (uncopied) error_ = fault_error_;

        return src.size() - uncopied;
    }

    /// \returns zero when no residual copy has occurred, or the injected negative error after a short copy
    [[nodiscard]] auto error() const noexcept -> int { return error_; }

private:
    std::byte* dst_;
    [[no_unique_address]] residual_copy_t residual_copy_;
    int fault_error_;
    int error_ = 0;
};

template <typename residual_copy_factory_t,
    typename t_product_t = copy_to_user_copier_t<typename residual_copy_factory_t::product_t>>
class copy_to_user_copier_factory_t
{
public:
    using product_t = t_product_t;

    constexpr copy_to_user_copier_factory_t(residual_copy_factory_t residual_copy_factory, int fault_error) noexcept
        : residual_copy_factory_{std::move(residual_copy_factory)}, fault_error_{fault_error}
    {
        assert(fault_error_ < 0);
    }

    constexpr auto operator()(void* dst) const noexcept -> product_t
    {
        return product_t{dst, residual_copy_factory_(), fault_error_};
    }

private:
    [[no_unique_address]] residual_copy_factory_t residual_copy_factory_;
    int fault_error_;
};

} // namespace crv
