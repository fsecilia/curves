// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "capture_file.hpp"
#include <crv/io/unique_fd.hpp>
#include <crv/kernel/input/capture_abi.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace crv {
namespace {

enum class exact_read_status_t
{
    complete,
    truncated,
    failed,
};

struct exact_read_result_t
{
    exact_read_status_t status;
    int system_error = 0;
};

[[nodiscard]] auto read_exact(int fd, void* destination, std::size_t size) -> exact_read_result_t
{
    auto* bytes = static_cast<std::byte*>(destination);
    auto offset = std::size_t{0};

    while (offset != size)
    {
        auto const result = read(fd, bytes + offset, size - offset);

        if (result > 0)
        {
            offset += static_cast<std::size_t>(result);
            continue;
        }

        if (result == 0) return {.status = exact_read_status_t::truncated};
        if (errno == EINTR) continue;

        return {
            .status = exact_read_status_t::failed,
            .system_error = errno != 0 ? errno : EIO,
        };
    }

    return {.status = exact_read_status_t::complete};
}

[[nodiscard]] auto header_read_error(exact_read_result_t result) -> capture_file_error_t
{
    if (result.status == exact_read_status_t::failed)
    {
        return {
            .code = capture_file_error_code_t::read_failed,
            .system_error = result.system_error,
        };
    }

    return {.code = capture_file_error_code_t::truncated_header};
}

} // namespace

auto open_capture_file(char const* path, std::size_t max_batch_value_count) -> capture_file_open_result_t
{
    auto fd = unique_fd_t{open(path, O_RDONLY | O_CLOEXEC)};

    if (!fd)
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::open_failed,
            .system_error = errno,
        });
    }

    static_assert(std::is_trivially_copyable_v<crv_capture_file_header_t>);

    auto header = crv_capture_file_header_t{};
    auto const read_result = read_exact(fd.get(), &header, sizeof(header));

    if (read_result.status != exact_read_status_t::complete) return std::unexpected(header_read_error(read_result));

    if (std::memcmp(header.magic, CRV_CAPTURE_FILE_MAGIC, sizeof(header.magic)) != 0)
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::invalid_magic,
        });
    }

    if (header.abi_version != CRV_CAPTURE_ABI_VERSION)
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::unsupported_abi_version,
        });
    }

    if (header.header_size != sizeof(crv_capture_file_header_t))
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::invalid_header_size,
        });
    }

    if (header.record_size != sizeof(crv_capture_event_t))
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::invalid_record_size,
        });
    }

    if (header.clock_id != CRV_CAPTURE_CLOCK_MONOTONIC)
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::unsupported_clock,
        });
    }

    // mvp is intentionally native-endian - marker detects an incompatible file; no byte swapping is attempted
    if (header.byte_order_marker != CRV_CAPTURE_BYTE_ORDER_MARKER)
    {
        return std::unexpected(capture_file_error_t{
            .code = capture_file_error_code_t::unsupported_byte_order,
        });
    }

    return capture_stream_t{fd_capture_source_t{std::move(fd)}, max_batch_value_count};
}

} // namespace crv
