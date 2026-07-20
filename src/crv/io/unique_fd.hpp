// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace crv {

struct default_fd_closer_t
{
    auto operator()(int fd) const noexcept -> void { static_cast<void>(close(fd)); }
};

template <typename closer_t>
concept is_closer = std::is_nothrow_move_constructible_v<closer_t> && std::is_nothrow_move_assignable_v<closer_t>
    && std::is_nothrow_invocable_r_v<void, closer_t&, int>;

namespace generic {

template <is_closer closer_t = default_fd_closer_t> class unique_fd_t
{
public:
    static constexpr auto disarmed_fd = int{-1};

    unique_fd_t() noexcept
        requires std::is_nothrow_default_constructible_v<closer_t>
    = default;

    explicit unique_fd_t(int fd) noexcept
        requires std::is_nothrow_default_constructible_v<closer_t>
        : unique_fd_t{fd, closer_t{}}
    {}

    explicit unique_fd_t(int fd, closer_t closer) noexcept : closer_{std::move(closer)}, fd_{fd} {}

    unique_fd_t(unique_fd_t const&) = delete;
    auto operator=(unique_fd_t const&) -> unique_fd_t& = delete;

    unique_fd_t(unique_fd_t&& src) noexcept : closer_{std::move(src).closer_}, fd_{src.release()} {}
    auto operator=(unique_fd_t&& src) noexcept -> unique_fd_t&
    {
        if (this == &src) return *this;

        reset(src.release(), std::move(src).closer_);

        return *this;
    }

    ~unique_fd_t() { reset(); }

    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }

    [[nodiscard]] auto get() const noexcept -> int { return fd_; }
    [[nodiscard]] auto operator*() const noexcept -> int { return get(); }

    [[nodiscard]] auto release() noexcept -> int { return std::exchange(fd_, disarmed_fd); }

    auto reset(int fd = disarmed_fd) noexcept -> void
    {
        auto const old_fd = std::exchange(fd_, fd);
        if (old_fd >= 0) closer_(old_fd);
    }

    auto reset(int fd, closer_t closer) noexcept -> void
    {
        reset(fd);
        closer_ = std::move(closer);
    }

private:
    [[no_unique_address]] closer_t closer_{};
    int fd_ = disarmed_fd;
};

} // namespace generic

using unique_fd_t = generic::unique_fd_t<default_fd_closer_t>;

} // namespace crv
