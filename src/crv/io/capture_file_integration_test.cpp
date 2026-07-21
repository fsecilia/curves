// SPDX-License-Identifier: MIT

#include "capture_file.hpp"
#include <crv/io/unique_fd.hpp>
#include <crv/kernel/input/capture/abi.h>
#include <crv/test/test.hpp>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace crv {
namespace {

class temporary_file_t
{
public:
    temporary_file_t()
    {
        char path[] = "/tmp/crv-capture-file-test-XXXXXX";
        fd_.reset(mkstemp(path));
        path_ = path;
    }

    temporary_file_t(temporary_file_t const&) = delete;
    auto operator=(temporary_file_t const&) -> temporary_file_t& = delete;

    ~temporary_file_t()
    {
        fd_.reset();
        if (!path_.empty()) static_cast<void>(::unlink(path_.c_str()));
    }

    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(fd_); }
    [[nodiscard]] auto path() const noexcept -> char const* { return path_.c_str(); }

    auto write(void const* data, std::size_t size) -> bool
    {
        auto const* bytes = static_cast<std::byte const*>(data);
        auto offset = std::size_t{0};

        while (offset != size)
        {
            auto const result = ::write(fd_.get(), bytes + offset, size - offset);

            if (result > 0)
            {
                offset += static_cast<std::size_t>(result);
                continue;
            }

            if (result < 0 && errno == EINTR) continue;
            return false;
        }

        return true;
    }

private:
    unique_fd_t fd_;
    std::string path_;
};

template <typename value_t> void append_object(std::vector<std::byte>& bytes, value_t const& value)
{
    auto const previous_size = bytes.size();
    bytes.resize(previous_size + sizeof(value));
    std::memcpy(bytes.data() + previous_size, &value, sizeof(value));
}

[[nodiscard]] auto valid_header() -> crv_capture_file_header_t
{
    auto header = crv_capture_file_header_t{};
    std::memcpy(header.magic, CRV_CAPTURE_FILE_MAGIC, sizeof(header.magic));
    header.abi_version = CRV_CAPTURE_ABI_VERSION;
    header.header_size = sizeof(header);
    header.record_size = sizeof(crv_capture_event_t);
    header.clock_id = CRV_CAPTURE_CLOCK_MONOTONIC;
    header.byte_order_marker = CRV_CAPTURE_BYTE_ORDER_MARKER;
    return header;
}

[[nodiscard]] auto event(std::uint64_t timestamp_ns, std::uint64_t sequence, std::uint16_t type, std::uint16_t code,
    std::int32_t value) -> crv_capture_event_t
{
    return {
        .timestamp_ns = timestamp_ns,
        .batch_sequence = sequence,
        .type = type,
        .code = code,
        .value = value,
    };
}

void expect_header_error(crv_capture_file_header_t const& header, capture_file_error_code_t expected)
{
    auto file = temporary_file_t{};
    ASSERT_TRUE(file);
    ASSERT_TRUE(file.write(&header, sizeof(header)));

    auto result = open_capture_file(file.path());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, expected);
}

TEST(capture_file, validates_header_and_returns_stream_at_first_record)
{
    auto bytes = std::vector<std::byte>{};
    auto const header = valid_header();
    append_object(bytes, header);
    append_object(bytes, event(1000, 0, 2, 0, 12));
    append_object(bytes, event(1000, 0, 0, 0, 0));

    auto file = temporary_file_t{};
    ASSERT_TRUE(file);
    ASSERT_TRUE(file.write(bytes.data(), bytes.size()));

    auto open_result = open_capture_file(file.path());
    ASSERT_TRUE(open_result);

    auto stream = std::move(*open_result);
    auto batch_result = stream.read_batch();
    ASSERT_TRUE(batch_result);
    ASSERT_TRUE(*batch_result);

    auto const batch = **batch_result;
    EXPECT_EQ(batch.timestamp_ns, 1000);
    EXPECT_EQ(batch.batch_sequence, 0);
    ASSERT_EQ(batch.values.size(), 2);
    EXPECT_EQ(batch.values[0].value, 12);

    auto end_result = stream.read_batch();
    ASSERT_TRUE(end_result);
    EXPECT_FALSE(*end_result);
}

TEST(capture_file, rejects_truncated_header)
{
    auto const header = valid_header();

    auto file = temporary_file_t{};
    ASSERT_TRUE(file);
    ASSERT_TRUE(file.write(&header, sizeof(header) - 1));

    auto result = open_capture_file(file.path());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, capture_file_error_code_t::truncated_header);
}

TEST(capture_file, rejects_invalid_magic)
{
    auto header = valid_header();
    header.magic[0] ^= 0xffu;
    expect_header_error(header, capture_file_error_code_t::invalid_magic);
}

TEST(capture_file, rejects_unsupported_abi_version)
{
    auto header = valid_header();
    ++header.abi_version;
    expect_header_error(header, capture_file_error_code_t::unsupported_abi_version);
}

TEST(capture_file, rejects_noncanonical_header_size)
{
    auto header = valid_header();
    ++header.header_size;
    expect_header_error(header, capture_file_error_code_t::invalid_header_size);
}

TEST(capture_file, rejects_unknown_record_size)
{
    auto header = valid_header();
    ++header.record_size;
    expect_header_error(header, capture_file_error_code_t::invalid_record_size);
}

TEST(capture_file, rejects_unknown_clock)
{
    auto header = valid_header();
    ++header.clock_id;
    expect_header_error(header, capture_file_error_code_t::unsupported_clock);
}

TEST(capture_file, detects_incompatible_native_byte_order)
{
    auto header = valid_header();
    header.byte_order_marker = 0x04030201u;
    expect_header_error(header, capture_file_error_code_t::unsupported_byte_order);
}

TEST(capture_file, reports_open_failure_with_errno)
{
    // open path that can not exist; generated using uuidgen
    auto result = open_capture_file("/06599bf0-b644-4940-bbb9-5074038450b9");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, capture_file_error_code_t::open_failed);
    EXPECT_NE(result.error().system_error, 0);
}

} // namespace
} // namespace crv
