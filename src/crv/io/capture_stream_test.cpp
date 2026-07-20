// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "capture_stream.hpp"
#include <crv/test/test.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <span>
#include <utility>
#include <vector>

namespace crv {
namespace {

struct source_step_t
{
    capture_source_read_status_t status;
    std::vector<std::byte> bytes;
    int system_error = 0;
    std::size_t offset = 0;
};

[[nodiscard]] auto data_step(std::vector<std::byte> bytes) -> source_step_t
{
    return source_step_t{
        .status = capture_source_read_status_t::data,
        .bytes = std::move(bytes),
        .system_error = 0,
        .offset = 0,
    };
}

[[nodiscard]] auto status_step(capture_source_read_status_t status, int system_error = 0) -> source_step_t
{
    return source_step_t{
        .status = status,
        .bytes = {},
        .system_error = system_error,
        .offset = 0,
    };
}

class scripted_source_t
{
public:
    explicit scripted_source_t(std::vector<source_step_t> steps) : steps_{std::move(steps)} {}

    auto read_some(std::span<std::byte> destination) -> capture_source_read_result_t
    {
        if (step_index_ == steps_.size()) return {.status = capture_source_read_status_t::end};

        auto& step = steps_[step_index_];

        if (step.status != capture_source_read_status_t::data)
        {
            ++step_index_;
            return {
                .status = step.status,
                .system_error = step.system_error,
            };
        }

        auto const remaining = step.bytes.size() - step.offset;
        auto const count = std::min(remaining, destination.size());
        std::memcpy(destination.data(), step.bytes.data() + step.offset, count);
        step.offset += count;

        if (step.offset == step.bytes.size()) ++step_index_;

        return {
            .status = capture_source_read_status_t::data,
            .size = count,
        };
    }

private:
    std::vector<source_step_t> steps_;
    std::size_t step_index_ = 0;
};

using stream_t = generic::capture_stream_t<scripted_source_t>;

template <typename value_t> void append_object(std::vector<std::byte>& bytes, value_t const& value)
{
    auto const previous_size = bytes.size();
    bytes.resize(previous_size + sizeof(value));
    std::memcpy(bytes.data() + previous_size, &value, sizeof(value));
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

[[nodiscard]] auto bytes_of(std::initializer_list<crv_capture_event_t> events) -> std::vector<std::byte>
{
    auto result = std::vector<std::byte>{};
    result.reserve(events.size() * sizeof(crv_capture_event_t));

    for (auto const& value : events) append_object(result, value);

    return result;
}

[[nodiscard]] auto fragmented_steps(std::vector<std::byte> const& bytes, std::initializer_list<std::size_t> sizes)
    -> std::vector<source_step_t>
{
    auto result = std::vector<source_step_t>{};
    auto offset = std::size_t{0};

    for (auto const requested_size : sizes)
    {
        if (offset == bytes.size()) break;

        auto const count = std::min(requested_size, bytes.size() - offset);
        result.push_back(data_step(std::vector<std::byte>{bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + count)}));
        offset += count;
    }

    if (offset != bytes.size())
    {
        result.push_back(
            data_step(std::vector<std::byte>{bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end()}));
    }

    return result;
}

TEST(capture_stream, reconstructs_fragmented_records_and_groups_adjacent_sequences)
{
    auto const raw = bytes_of({
        event(1000, 10, 2, 0, 17),
        event(1000, 10, 2, 1, -9),
        event(1000, 10, 0, 0, 0),
        event(1250, 14, 2, 0, 3),
    });

    auto steps = fragmented_steps(raw, {1, 2, 5, 7, 11, 3, 19});
    steps.push_back(status_step(capture_source_read_status_t::end));

    auto stream = stream_t{scripted_source_t{std::move(steps)}};

    auto first_result = stream.read_batch();
    ASSERT_TRUE(first_result);
    ASSERT_TRUE(*first_result);

    auto const first = **first_result;
    EXPECT_EQ(first.timestamp_ns, 1000);
    EXPECT_EQ(first.batch_sequence, 10);
    ASSERT_EQ(first.values.size(), 3);
    EXPECT_EQ(first.values[0].type, 2);
    EXPECT_EQ(first.values[0].code, 0);
    EXPECT_EQ(first.values[0].value, 17);
    EXPECT_EQ(first.values[1].value, -9);
    EXPECT_EQ(first.values[2].type, 0);

    auto second_result = stream.read_batch();
    ASSERT_TRUE(second_result);
    ASSERT_TRUE(*second_result);

    auto const second = **second_result;
    EXPECT_EQ(second.timestamp_ns, 1250);
    EXPECT_EQ(second.batch_sequence, 14); // sequence gaps are data, not decoder failures
    ASSERT_EQ(second.values.size(), 1);
    EXPECT_EQ(second.values[0].value, 3);

    auto end_result = stream.read_batch();
    ASSERT_TRUE(end_result);
    EXPECT_FALSE(*end_result);
}

TEST(capture_stream, interruption_preserves_partially_assembled_batch_for_retry)
{
    auto steps = std::vector<source_step_t>{};
    steps.push_back(data_step(bytes_of({event(1000, 7, 2, 0, 1)})));
    steps.push_back(status_step(capture_source_read_status_t::interrupted, EINTR));
    steps.push_back(data_step(bytes_of({event(1000, 7, 2, 1, 2), event(1100, 8, 0, 0, 0)})));
    steps.push_back(status_step(capture_source_read_status_t::end));

    auto stream = stream_t{scripted_source_t{std::move(steps)}};

    auto interrupted = stream.read_batch();
    ASSERT_FALSE(interrupted);
    EXPECT_EQ(interrupted.error().code, capture_stream_error_code_t::interrupted);
    EXPECT_EQ(interrupted.error().system_error, EINTR);

    auto resumed = stream.read_batch();
    ASSERT_TRUE(resumed);
    ASSERT_TRUE(*resumed);
    ASSERT_EQ((**resumed).values.size(), 2);
    EXPECT_EQ((**resumed).values[0].value, 1);
    EXPECT_EQ((**resumed).values[1].value, 2);

    auto final_batch = stream.read_batch();
    ASSERT_TRUE(final_batch);
    ASSERT_TRUE(*final_batch);
    EXPECT_EQ((**final_batch).batch_sequence, 8);
}

TEST(capture_stream, delivers_final_batch_before_reporting_source_disconnect)
{
    auto steps = std::vector<source_step_t>{};
    steps.push_back(data_step(bytes_of({event(1000, 0, 2, 0, 1), event(1000, 0, 0, 0, 0)})));
    steps.push_back(status_step(capture_source_read_status_t::disconnected, ENODEV));

    auto stream = stream_t{scripted_source_t{std::move(steps)}};

    auto batch = stream.read_batch();
    ASSERT_TRUE(batch);
    ASSERT_TRUE(*batch);
    EXPECT_EQ((**batch).values.size(), 2);

    auto disconnected = stream.read_batch();
    ASSERT_FALSE(disconnected);
    EXPECT_EQ(disconnected.error().code, capture_stream_error_code_t::source_disconnected);
    EXPECT_EQ(disconnected.error().system_error, ENODEV);
}

TEST(capture_stream, rejects_different_timestamps_within_one_sequence)
{
    auto steps = std::vector<source_step_t>{};
    steps.push_back(data_step(bytes_of({event(1000, 3, 2, 0, 1), event(1001, 3, 2, 1, 2)})));
    steps.push_back(status_step(capture_source_read_status_t::end));

    auto stream = stream_t{scripted_source_t{std::move(steps)}};
    auto result = stream.read_batch();

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, capture_stream_error_code_t::inconsistent_batch_timestamp);
}

TEST(capture_stream, rejects_a_partial_final_record)
{
    auto raw = bytes_of({event(1000, 0, 2, 0, 1)});
    raw.push_back(std::byte{0x12});
    raw.push_back(std::byte{0x34});

    auto steps = std::vector<source_step_t>{};
    steps.push_back(data_step(std::move(raw)));
    steps.push_back(status_step(capture_source_read_status_t::end));

    auto stream = stream_t{scripted_source_t{std::move(steps)}};
    auto result = stream.read_batch();

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, capture_stream_error_code_t::truncated_record);
}

TEST(capture_stream, enforces_the_configured_batch_limit)
{
    auto steps = std::vector<source_step_t>{};
    steps.push_back(data_step(bytes_of({event(1000, 0, 2, 0, 1), event(1000, 0, 2, 1, 2)})));
    steps.push_back(status_step(capture_source_read_status_t::end));

    auto stream = stream_t{scripted_source_t{std::move(steps)}, 1};
    auto result = stream.read_batch();

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, capture_stream_error_code_t::batch_too_large);
}

} // namespace
} // namespace crv
