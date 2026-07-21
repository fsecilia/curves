// SPDX-License-Identifier: MIT

/// \file
/// \brief blocking user-mode harness for the raw input capture stream
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/io/unique_fd.hpp>
#include <crv/kernel/input/capture/abi.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace crv {
namespace {

constexpr char default_device_path[] = "/dev/crv-input-capture";
constexpr auto read_event_count = std::size_t{4096};

auto volatile stop_requested = std::sig_atomic_t{0};
extern "C" void stop_signal_handler(int)
{
    stop_requested = 1;
}

auto install_signal_handlers() -> bool
{
    struct sigaction action{};
    action.sa_handler = stop_signal_handler;
    sigemptyset(&action.sa_mask);

    // Deliberately omit SA_RESTART so Ctrl-C interrupts a blocking read()
    action.sa_flags = 0;

    return sigaction(SIGINT, &action, nullptr) == 0 && sigaction(SIGTERM, &action, nullptr) == 0;
}

auto write_all(int fd, void const* data, std::size_t size) -> bool
{
    auto const* bytes = static_cast<std::byte const*>(data);
    auto written = std::size_t{0};

    while (written != size)
    {
        auto const result = write(fd, bytes + written, size - written);

        if (result > 0)
        {
            written += static_cast<std::size_t>(result);
            continue;
        }

        if (result < 0 && errno == EINTR) continue;
        if (result == 0) errno = EIO;

        return false;
    }

    return true;
}

auto sync_file(int fd) -> bool
{
    for (;;)
    {
        if (fsync(fd) == 0) return true;
        if (errno != EINTR) return false;
    }
}

void print_open_error(char const* path)
{
    switch (errno)
    {
        case EBUSY: std::fprintf(stderr, "%s: capture stream is already open\n", path); break;
        case ENODEV: std::fprintf(stderr, "%s: no matching REL_X/REL_Y input device is attached\n", path); break;
        case EACCES: std::fprintf(stderr, "%s: permission denied; the MVP device node is mode 0400\n", path); break;
        default: std::fprintf(stderr, "%s: open failed: %s\n", path, std::strerror(errno)); break;
    }
}

struct sequence_tracker_t
{
    bool initialized = false;
    crv_capture_u64 last = 0;
    crv_capture_u64 missing_batch_count = 0;

    void observe(crv_capture_u64 sequence)
    {
        if (!initialized)
        {
            initialized = true;
            last = sequence;
            return;
        }

        if (sequence == last) return;

        auto const expected = last + 1;
        if (sequence != expected)
        {
            auto const missing_count = sequence - expected;
            missing_batch_count += missing_count;

            std::fprintf(stderr,
                "sequence gap: expected %llu, received %llu "
                "(missing %llu batch%s)\n",
                static_cast<unsigned long long>(expected), static_cast<unsigned long long>(sequence),
                static_cast<unsigned long long>(missing_count), missing_count == 1 ? "" : "es");
        }

        last = sequence;
    }
};

auto run_capture(char const* output_path, char const* device_path) -> int
{
    auto const device = unique_fd_t{open(device_path, O_RDONLY | O_CLOEXEC)};

    if (!device)
    {
        print_open_error(device_path);
        return EXIT_FAILURE;
    }

    auto const output = unique_fd_t{open(output_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644)};

    if (!output)
    {
        std::fprintf(stderr, "%s: open failed: %s\n", output_path, std::strerror(errno));
        return EXIT_FAILURE;
    }

    auto header = crv_capture_file_header_t{};
    std::memcpy(header.magic, CRV_CAPTURE_FILE_MAGIC, sizeof(header.magic));
    header.abi_version = CRV_CAPTURE_ABI_VERSION;
    header.header_size = sizeof(header);
    header.record_size = sizeof(struct crv_capture_event_t);
    header.clock_id = CRV_CAPTURE_CLOCK_MONOTONIC;
    header.byte_order_marker = CRV_CAPTURE_BYTE_ORDER_MARKER;

    if (!write_all(output.get(), &header, sizeof(header)))
    {
        std::fprintf(stderr, "%s: header write failed: %s\n", output_path, std::strerror(errno));
        return EXIT_FAILURE;
    }

    auto events = std::array<crv_capture_event_t, read_event_count>{};
    auto sequences = sequence_tracker_t{};
    auto total_events = std::uint64_t{0};
    auto device_disconnected = false;
    auto failed = false;

    std::fprintf(stderr, "capturing %s to %s; press Ctrl-C to stop\n", device_path, output_path);

    while (!stop_requested)
    {
        auto const result = read(device.get(), events.data(), events.size() * sizeof(events[0]));

        if (result > 0)
        {
            auto const bytes = static_cast<std::size_t>(result);

            if (bytes % sizeof(events[0]) != 0)
            {
                std::fprintf(stderr, "driver returned a non-record-aligned read: %zu bytes\n", bytes);
                failed = true;
                break;
            }

            auto const count = bytes / sizeof(events[0]);
            for (auto index = std::size_t{0}; index < count; ++index) sequences.observe(events[index].batch_sequence);

            if (!write_all(output.get(), events.data(), bytes))
            {
                std::fprintf(stderr, "%s: data write failed: %s\n", output_path, std::strerror(errno));
                failed = true;
                break;
            }

            total_events += count;
            continue;
        }

        if (result == 0)
        {
            std::fprintf(stderr, "capture device returned unexpected EOF\n");
            failed = true;
            break;
        }

        if (errno == EINTR)
        {
            if (stop_requested) break;
            continue;
        }

        if (errno == ENODEV)
        {
            std::fprintf(stderr, "capture source disconnected; buffered records were drained\n");
            device_disconnected = true;
            break;
        }

        std::fprintf(stderr, "%s: read failed: %s\n", device_path, std::strerror(errno));
        failed = true;
        break;
    }

    if (!sync_file(output.get()))
    {
        std::fprintf(stderr, "%s: fsync failed: %s\n", output_path, std::strerror(errno));
        failed = true;
    }

    std::fprintf(stderr, "saved %llu events; detected %llu missing batches%s\n",
        static_cast<unsigned long long>(total_events), static_cast<unsigned long long>(sequences.missing_batch_count),
        device_disconnected ? "; source disconnected" : "");

    return failed || device_disconnected ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace
} // namespace crv

auto main(int argc, char** argv) -> int
{
    using namespace crv;

    static_assert(std::is_trivially_copyable_v<crv_capture_file_header_t>);
    static_assert(std::is_trivially_copyable_v<crv_capture_event_t>);

    if (argc < 2 || 3 < argc)
    {
        std::fprintf(stderr, "usage: %s OUTPUT_FILE [CAPTURE_DEVICE]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!install_signal_handlers())
    {
        std::fprintf(stderr, "failed to install signal handlers: %s\n", std::strerror(errno));
        return EXIT_FAILURE;
    }

    char const* const output_path = argv[1];
    char const* const device_path = argc == 3 ? argv[2] : default_device_path;

    return run_capture(output_path, device_path);
}
