// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "linux_io.hpp"
#include <crv/kernel/control/ioctl.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace crv::pipeline::control {

namespace {

struct ioctl_result_t
{
    auto operator()(int operation) const -> linux_io_t::result_t
    {
        if (operation != -1) return {};
        return std::unexpected{int_t{errno}};
    }
};

} // namespace

auto linux_io_t::open() -> open_result_t
{
    auto const fd = ::open(CRV_CONTROL_DEVICE_PATH, O_RDWR | O_CLOEXEC);
    if (fd == -1) return std::unexpected{int_t{errno}};
    return linux_io_t{unique_fd_t{fd}};
}

auto linux_io_t::get_device(crv_control_device_v1_t& request) const -> result_t
{
    return ioctl_result_t{}(::ioctl(*fd_, CRV_CONTROL_IOCTL_GET_DEVICE_V1, &request));
}

auto linux_io_t::apply(crv_control_apply_v1_t const& request) const -> result_t
{
    return ioctl_result_t{}(::ioctl(*fd_, CRV_CONTROL_IOCTL_APPLY_V1, &request));
}

} // namespace crv::pipeline::control
