// SPDX-License-Identifier: MIT

/// \file
/// \brief linux control-endpoint io
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/io/unique_fd.hpp>
#include <crv/kernel/control/abi.h>
#include <expected>
#include <utility>

namespace crv::pipeline::control {

class linux_io_t
{
public:
    using result_t = std::expected<void, int_t>;
    using open_result_t = std::expected<linux_io_t, int_t>;

    [[nodiscard]] static auto open() -> open_result_t;

    auto get_device(crv_control_device_v1_t& request) const -> result_t;
    auto apply(crv_control_apply_v1_t const& request) const -> result_t;

private:
    explicit linux_io_t(unique_fd_t fd) noexcept : fd_{std::move(fd)} {}

    unique_fd_t fd_;
};

} // namespace crv::pipeline::control
