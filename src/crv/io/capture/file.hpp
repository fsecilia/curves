// SPDX-License-Identifier: MIT

/// \file
/// \brief opens raw linux input-value capture files
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/io/capture/stream.hpp>

#include <cerrno>
#include <expected>
#include <fcntl.h>
#include <utility>

namespace crv {

enum class capture_file_error_code_t
{
    open_failed,
};

struct capture_file_error_t
{
    capture_file_error_code_t code;
    int system_error = 0;
};
using capture_file_open_result_t = std::expected<capture_stream_t, capture_file_error_t>;

/// opens a capture file and returns a decoder positioned at byte zero
[[nodiscard]] inline auto open_capture_file(char const* path, capture_stream_limits_t limits = {})
    -> capture_file_open_result_t
{
    auto fd = unique_fd_t{open(path, O_RDONLY | O_CLOEXEC)};

    if (!fd)
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::open_failed,
            .system_error = errno != 0 ? errno : EIO,
        });
    }

    return capture_stream_t{
        fd_capture_source_t{std::move(fd)},
        limits,
    };
}

} // namespace crv
