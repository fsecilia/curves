// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stream.hpp"
#include <crv/test/test.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace crv {
namespace {

struct byte_sink_state_t
{
    std::vector<std::byte> bytes;
    std::vector<std::size_t> requested_write_sizes;
    std::size_t capacity{};
};

class bounded_byte_sink_t
{
public:
    explicit bounded_byte_sink_t(byte_sink_state_t& state) noexcept : state_{&state} {}

    [[nodiscard]] auto try_write_exact(std::span<std::byte const> bytes) noexcept -> bool
    {
        state_->requested_write_sizes.push_back(bytes.size());

        if (state_->bytes.size() > state_->capacity || bytes.size() > state_->capacity - state_->bytes.size())
        {
            return false;
        }

        state_->bytes.insert(state_->bytes.end(), bytes.begin(), bytes.end());
        return true;
    }

private:
    byte_sink_state_t* state_;
};

template <typename value_t> [[nodiscard]] auto load(std::span<std::byte const> bytes, std::size_t offset) -> value_t
{
    EXPECT_LE(offset + sizeof(value_t), bytes.size());

    auto result = value_t{};
    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

constexpr auto test_values = std::array{
    crv_input_value_t{
        .type = CRV_EV_REL,
        .code = CRV_REL_X,
        .value = -123,
    },
    crv_input_value_t{
        .type = CRV_EV_REL,
        .code = CRV_REL_Y,
        .value = 456,
    },
    crv_input_value_t{
        .type = CRV_EV_SYN,
        .code = CRV_SYN_REPORT,
        .value = 0,
    },
};

TEST(capture_stream_producer, emits_header_then_raw_batch)
{
    constexpr auto payload_size = test_values.size() * sizeof(crv_input_value_t);
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t) + payload_size;

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sizeof(crv_capture_stream_header_t) + frame_size,
    };

    auto scratch = std::array<std::byte, frame_size>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());

    ASSERT_EQ(producer.try_push(123'456'789, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    auto const bytes = std::span<std::byte const>{sink_state.bytes};

    ASSERT_EQ(bytes.size(), sizeof(crv_capture_stream_header_t) + frame_size);

    auto const stream_header = load<crv_capture_stream_header_t>(bytes, 0);

    EXPECT_EQ(stream_header.magic, CRV_CAPTURE_STREAM_MAGIC);
    EXPECT_EQ(stream_header.abi_major, CRV_CAPTURE_ABI_MAJOR);
    EXPECT_EQ(stream_header.abi_minor, CRV_CAPTURE_ABI_MINOR);
    EXPECT_EQ(stream_header.header_size, sizeof(crv_capture_stream_header_t));
    EXPECT_EQ(stream_header.batch_header_size, sizeof(crv_capture_batch_header_t));
    EXPECT_EQ(stream_header.input_value_size, sizeof(crv_input_value_t));

    constexpr auto batch_offset = sizeof(crv_capture_stream_header_t);

    auto const batch_header = load<crv_capture_batch_header_t>(bytes, batch_offset);

    EXPECT_EQ(batch_header.timestamp_ns, 123'456'789);
    EXPECT_EQ(batch_header.sequence, 0);
    EXPECT_EQ(batch_header.count, test_values.size());
    EXPECT_EQ(batch_header.capacity, 64);

    auto const payload_offset = batch_offset + sizeof(crv_capture_batch_header_t);

    ASSERT_EQ(std::memcmp(bytes.data() + payload_offset, test_values.data(), payload_size), 0);

    EXPECT_EQ(producer.state().next_sequence, 1);
    EXPECT_EQ(producer.state().batches_written, 1);
    EXPECT_EQ(producer.state().batches_dropped, 0);
    EXPECT_EQ(producer.state().bytes_written, bytes.size());

    ASSERT_EQ(sink_state.requested_write_sizes.size(), 2);
    EXPECT_EQ(sink_state.requested_write_sizes[0], sizeof(crv_capture_stream_header_t));
    EXPECT_EQ(sink_state.requested_write_sizes[1], frame_size);
}

TEST(capture_stream_producer, queue_full_drops_whole_batch_and_exposes_gap)
{
    constexpr auto payload_size = test_values.size() * sizeof(crv_input_value_t);
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t) + payload_size;

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sizeof(crv_capture_stream_header_t) + frame_size,
    };

    auto scratch = std::array<std::byte, frame_size>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());

    ASSERT_EQ(producer.try_push(100, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    auto const size_before_failure = sink_state.bytes.size();

    EXPECT_EQ(producer.try_push(200, test_values.data(), test_values.size(), 64), CRV_CAPTURE_QUEUE_FULL);

    EXPECT_EQ(sink_state.bytes.size(), size_before_failure);
    EXPECT_EQ(producer.state().next_sequence, 2);
    EXPECT_EQ(producer.state().batches_dropped, 1);

    sink_state.capacity += frame_size;

    ASSERT_EQ(producer.try_push(300, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    auto const bytes = std::span<std::byte const>{sink_state.bytes};

    auto const second_stored_batch
        = load<crv_capture_batch_header_t>(bytes, sizeof(crv_capture_stream_header_t) + frame_size);

    EXPECT_EQ(second_stored_batch.timestamp_ns, 300);
    EXPECT_EQ(second_stored_batch.sequence, 2);
}

TEST(capture_stream_producer, inadequate_scratch_writes_nothing)
{
    constexpr auto payload_size = test_values.size() * sizeof(crv_input_value_t);
    constexpr auto required_frame_size = sizeof(crv_capture_batch_header_t) + payload_size;

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = 4096,
    };

    auto scratch = std::array<std::byte, required_frame_size - 1>{};

    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());

    auto const size_before = sink_state.bytes.size();

    EXPECT_EQ(producer.try_push(100, test_values.data(), test_values.size(), 64), CRV_CAPTURE_SCRATCH_TOO_SMALL);

    EXPECT_EQ(sink_state.bytes.size(), size_before);
    EXPECT_EQ(producer.state().next_sequence, 0);
}

void expect_producer_state_eq(crv_capture_producer_state_t const& actual, crv_capture_producer_state_t const& expected)
{
    EXPECT_EQ(actual.next_sequence, expected.next_sequence);
    EXPECT_EQ(actual.batches_written, expected.batches_written);
    EXPECT_EQ(actual.batches_dropped, expected.batches_dropped);
    EXPECT_EQ(actual.bytes_written, expected.bytes_written);
}

TEST(capture_stream_producer, writes_each_successful_batch_with_one_exact_sink_operation)
{
    constexpr auto payload_size = test_values.size() * sizeof(crv_input_value_t);
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t) + payload_size;

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sizeof(crv_capture_stream_header_t) + frame_size,
    };

    auto scratch = std::array<std::byte, frame_size>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());

    ASSERT_EQ(sink_state.requested_write_sizes.size(), 1);
    EXPECT_EQ(sink_state.requested_write_sizes[0], sizeof(crv_capture_stream_header_t));

    auto const size_before = sink_state.bytes.size();

    ASSERT_EQ(producer.try_push(123, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    ASSERT_EQ(sink_state.requested_write_sizes.size(), 2);
    EXPECT_EQ(sink_state.requested_write_sizes[1], frame_size);
    EXPECT_EQ(sink_state.bytes.size() - size_before, frame_size);
}

TEST(capture_stream_producer, preserves_unknown_records_and_extreme_signed_values_byte_for_byte)
{
    constexpr auto values = std::array{
        crv_input_value_t{
            .type = static_cast<crv_input_u16_t>(0xffff),
            .code = static_cast<crv_input_u16_t>(0x8123),
            .value = std::numeric_limits<crv_input_s32_t>::min(),
        },
        crv_input_value_t{
            .type = static_cast<crv_input_u16_t>(0x7abc),
            .code = static_cast<crv_input_u16_t>(0xfffe),
            .value = std::numeric_limits<crv_input_s32_t>::max(),
        },
    };

    constexpr auto payload_size = values.size() * sizeof(crv_input_value_t);
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t) + payload_size;

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sizeof(crv_capture_stream_header_t) + frame_size,
    };

    auto scratch = std::array<std::byte, frame_size>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());
    ASSERT_EQ(producer.try_push(987'654'321, values.data(), values.size(), 4096), CRV_CAPTURE_PUSHED);

    auto const bytes = std::span<std::byte const>{sink_state.bytes};

    constexpr auto batch_offset = sizeof(crv_capture_stream_header_t);
    constexpr auto payload_offset = batch_offset + sizeof(crv_capture_batch_header_t);

    ASSERT_EQ(bytes.size(), payload_offset + payload_size);
    EXPECT_EQ(std::memcmp(bytes.data() + payload_offset, values.data(), payload_size), 0);
}

TEST(capture_stream_producer, accepts_empty_batch_with_null_values)
{
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t);

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sizeof(crv_capture_stream_header_t) + frame_size,
    };

    auto scratch = std::array<std::byte, frame_size>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());
    ASSERT_EQ(producer.try_push(555, nullptr, 0, 64), CRV_CAPTURE_PUSHED);

    auto const bytes = std::span<std::byte const>{sink_state.bytes};

    ASSERT_EQ(bytes.size(), sizeof(crv_capture_stream_header_t) + frame_size);

    auto const batch_header = load<crv_capture_batch_header_t>(bytes, sizeof(crv_capture_stream_header_t));

    EXPECT_EQ(batch_header.timestamp_ns, 555);
    EXPECT_EQ(batch_header.sequence, 0);
    EXPECT_EQ(batch_header.count, 0);
    EXPECT_EQ(batch_header.capacity, 64);

    ASSERT_EQ(sink_state.requested_write_sizes.size(), 2);
    EXPECT_EQ(sink_state.requested_write_sizes[1], frame_size);

    EXPECT_EQ(producer.state().next_sequence, 1);
    EXPECT_EQ(producer.state().batches_written, 1);
    EXPECT_EQ(producer.state().batches_dropped, 0);
    EXPECT_EQ(producer.state().bytes_written, bytes.size());
}

TEST(capture_stream_producer, invalid_input_does_not_write_or_change_state)
{
    auto run_invalid_case
        = [](char const* description, crv_input_value_t const* values, std::size_t count, std::size_t capacity) {
              SCOPED_TRACE(description);

              auto sink_state = byte_sink_state_t{
                  .bytes = {},
                  .requested_write_sizes = {},
                  .capacity = 4096,
              };

              auto scratch = std::array<std::byte, 4096>{};
              auto producer_state = crv_capture_producer_state_t{};

              auto producer = capture_stream_producer_t{
                  bounded_byte_sink_t{sink_state},
                  std::span{scratch},
                  producer_state,
              };

              ASSERT_TRUE(producer.begin_session());

              auto const state_before = producer.state();
              auto const byte_count_before = sink_state.bytes.size();
              auto const write_count_before = sink_state.requested_write_sizes.size();

              EXPECT_EQ(producer.try_push(100, values, count, capacity), CRV_CAPTURE_INVALID_INPUT);

              EXPECT_EQ(sink_state.bytes.size(), byte_count_before);
              EXPECT_EQ(sink_state.requested_write_sizes.size(), write_count_before);
              expect_producer_state_eq(producer.state(), state_before);
          };

    run_invalid_case("count exceeds capacity", test_values.data(), 1, 0);
    run_invalid_case("nonempty input has null values", nullptr, 1, 1);

    if constexpr (sizeof(std::size_t) > sizeof(crv_capture_u32_t))
    {
        constexpr auto wire_max = std::numeric_limits<crv_capture_u32_t>::max();
        constexpr auto too_large = static_cast<std::size_t>(wire_max) + 1;

        run_invalid_case("count exceeds wire representation", test_values.data(), too_large, too_large);
        run_invalid_case("capacity exceeds wire representation", nullptr, 0, too_large);
    }
}

TEST(capture_stream_producer, batch_size_rejects_arithmetic_overflow)
{
    EXPECT_FALSE(capture_batch_size(std::numeric_limits<std::size_t>::max()).has_value());
}

TEST(capture_stream_producer, scratch_failure_does_not_call_sink_or_change_state)
{
    constexpr auto required_frame_size
        = sizeof(crv_capture_batch_header_t) + test_values.size() * sizeof(crv_input_value_t);

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = 4096,
    };

    auto scratch = std::array<std::byte, required_frame_size - 1>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());

    auto const state_before = producer.state();
    auto const byte_count_before = sink_state.bytes.size();
    auto const write_count_before = sink_state.requested_write_sizes.size();

    EXPECT_EQ(producer.try_push(100, test_values.data(), test_values.size(), 64), CRV_CAPTURE_SCRATCH_TOO_SMALL);

    EXPECT_EQ(sink_state.bytes.size(), byte_count_before);
    EXPECT_EQ(sink_state.requested_write_sizes.size(), write_count_before);
    expect_producer_state_eq(producer.state(), state_before);
}

TEST(capture_stream_producer, statistics_track_success_drop_and_sequence_gap)
{
    constexpr auto payload_size = test_values.size() * sizeof(crv_input_value_t);
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t) + payload_size;

    auto sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sizeof(crv_capture_stream_header_t) + frame_size,
    };

    auto scratch = std::array<std::byte, frame_size>{};
    auto producer_state = crv_capture_producer_state_t{};

    auto producer = capture_stream_producer_t{
        bounded_byte_sink_t{sink_state},
        std::span{scratch},
        producer_state,
    };

    ASSERT_TRUE(producer.begin_session());

    ASSERT_EQ(producer.try_push(100, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);
    ASSERT_EQ(producer.try_push(200, test_values.data(), test_values.size(), 64), CRV_CAPTURE_QUEUE_FULL);

    sink_state.capacity += frame_size;

    ASSERT_EQ(producer.try_push(300, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    EXPECT_EQ(producer.state().next_sequence, 3);
    EXPECT_EQ(producer.state().batches_written, 2);
    EXPECT_EQ(producer.state().batches_dropped, 1);
    EXPECT_EQ(producer.state().bytes_written, sizeof(crv_capture_stream_header_t) + 2 * frame_size);

    auto const bytes = std::span<std::byte const>{sink_state.bytes};

    auto const second_stored_header
        = load<crv_capture_batch_header_t>(bytes, sizeof(crv_capture_stream_header_t) + frame_size);

    EXPECT_EQ(second_stored_header.timestamp_ns, 300);
    EXPECT_EQ(second_stored_header.sequence, 2);
}

TEST(capture_stream_producer, new_session_restarts_sequence_at_zero)
{
    constexpr auto payload_size = test_values.size() * sizeof(crv_input_value_t);
    constexpr auto frame_size = sizeof(crv_capture_batch_header_t) + payload_size;
    constexpr auto sink_capacity = sizeof(crv_capture_stream_header_t) + frame_size;

    auto producer_state = crv_capture_producer_state_t{};
    auto first_scratch = std::array<std::byte, frame_size>{};

    auto first_sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sink_capacity,
    };

    auto first_producer = capture_stream_producer_t{
        bounded_byte_sink_t{first_sink_state},
        std::span{first_scratch},
        producer_state,
    };

    ASSERT_TRUE(first_producer.begin_session());
    ASSERT_EQ(first_producer.try_push(100, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    EXPECT_EQ(producer_state.next_sequence, 1);

    auto second_scratch = std::array<std::byte, frame_size>{};

    auto second_sink_state = byte_sink_state_t{
        .bytes = {},
        .requested_write_sizes = {},
        .capacity = sink_capacity,
    };

    auto second_producer = capture_stream_producer_t{
        bounded_byte_sink_t{second_sink_state},
        std::span{second_scratch},
        producer_state,
    };

    ASSERT_TRUE(second_producer.begin_session());

    EXPECT_EQ(producer_state.next_sequence, 0);
    EXPECT_EQ(producer_state.batches_written, 0);
    EXPECT_EQ(producer_state.batches_dropped, 0);
    EXPECT_EQ(producer_state.bytes_written, sizeof(crv_capture_stream_header_t));

    ASSERT_EQ(second_producer.try_push(200, test_values.data(), test_values.size(), 64), CRV_CAPTURE_PUSHED);

    auto const bytes = std::span<std::byte const>{second_sink_state.bytes};

    auto const first_batch_header = load<crv_capture_batch_header_t>(bytes, sizeof(crv_capture_stream_header_t));

    EXPECT_EQ(first_batch_header.timestamp_ns, 200);
    EXPECT_EQ(first_batch_header.sequence, 0);
}

} // namespace
} // namespace crv
