// SPDX-License-Identifier: MIT

/// \file
/// \brief opens and validates raw linux input capture files
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/io/capture_stream.hpp>
#include <cstddef>
#include <expected>

namespace crv {

enum class capture_file_error_code_t
{
    open_failed,
    read_failed,
    truncated_header,
    invalid_magic,
    unsupported_abi_version,
    invalid_header_size,
    invalid_record_size,
    unsupported_clock,
    unsupported_byte_order,
};

struct capture_file_error_t
{
    capture_file_error_code_t code;
    int system_error = 0;
};

using capture_file_open_result_t = std::expected<capture_stream_t, capture_file_error_t>;

/// opens and validates capture file
///
/// This function opens a capture file, validates and consumes its header, then returns common fd-backed event stream
/// positioned at its first capture record.
[[nodiscard]] auto open_capture_file(char const* path,
    std::size_t max_batch_value_count = capture_stream_t::default_max_batch_value_count) -> capture_file_open_result_t;

} // namespace crv
