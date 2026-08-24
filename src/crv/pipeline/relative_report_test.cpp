// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "relative_report.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <concepts>
#include <utility>

namespace crv::pipeline {
namespace {

static_assert(!std::copy_constructible<relative_report_t>);
static_assert(!std::move_constructible<relative_report_t>);

struct relative_report_test_t : Test
{
    static constexpr auto rel(input_value_t::code_rel_t code, input_value_t::value_t value) noexcept -> input_value_t
    {
        return {
            .type = input_value_t::type_t::rel,
            .code = static_cast<input_value_t::code_t>(code),
            .value = value,
        };
    }

    static constexpr auto syn() noexcept -> input_value_t
    {
        return {
            .type = input_value_t::type_t::syn,
            .code = static_cast<input_value_t::code_t>(input_value_t::code_syn_t::report),
            .value = 0,
        };
    }

    template <std::size_t size>
    static auto matches(input_frame_t const& frame, std::array<input_value_t, size> const& expected) noexcept -> bool
    {
        if (frame.count() != size) return false;
        for (auto index = std::size_t{0}; index < size; ++index)
            if (frame.load(index) != expected[index]) return false;
        return true;
    }
};

TEST_F(relative_report_test_t, invalid_frame_is_invalid_report)
{
    auto storage = std::array<input_value_t, 2>{rel(input_value_t::code_rel_t::x, 17), {}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 1};

    EXPECT_FALSE(relative_report_t{frame}.valid());
}

TEST_F(relative_report_test_t, inspection_uses_original_axes_and_zero_for_missing_axis)
{
    auto storage = std::array<input_value_t, 3>{rel(input_value_t::code_rel_t::x, 17), syn(), {}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 2};

    auto const report = relative_report_t{frame};

    EXPECT_TRUE(report.valid());
    EXPECT_EQ(report.x(), 17);
    EXPECT_EQ(report.y(), 0);
}

TEST_F(relative_report_test_t, duplicate_axis_is_invalid)
{
    auto storage = std::array{
        rel(input_value_t::code_rel_t::x, 17), rel(input_value_t::code_rel_t::x, 23), syn(), input_value_t{}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 3};

    EXPECT_FALSE(relative_report_t{frame}.valid());
}

TEST_F(relative_report_test_t, stores_existing_axes_after_inspection)
{
    auto storage = std::array{
        rel(input_value_t::code_rel_t::x, 17), rel(input_value_t::code_rel_t::y, -9), syn(), input_value_t{}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 3};
    auto report = relative_report_t{frame};

    ASSERT_TRUE(std::move(report).try_store(-3, 4));

    EXPECT_TRUE(
        matches(frame, std::array{rel(input_value_t::code_rel_t::x, -3), rel(input_value_t::code_rel_t::y, 4), syn()}));
}

TEST_F(relative_report_test_t, appends_missing_axis_then_erases_existing_axis_that_became_zero)
{
    auto storage = std::array{rel(input_value_t::code_rel_t::x, 17), syn(), input_value_t{}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 2};
    auto report = relative_report_t{frame};

    ASSERT_TRUE(std::move(report).try_store(0, 6));

    EXPECT_TRUE(matches(frame, std::array{rel(input_value_t::code_rel_t::y, 6), syn()}));
}

TEST_F(relative_report_test_t, erases_both_existing_axes_when_both_quantize_to_zero)
{
    auto storage = std::array{
        rel(input_value_t::code_rel_t::x, 17), rel(input_value_t::code_rel_t::y, -9), syn(), input_value_t{}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 3};
    auto report = relative_report_t{frame};

    ASSERT_TRUE(std::move(report).try_store(0, 0));

    EXPECT_TRUE(matches(frame, std::array{syn()}));
}

TEST_F(relative_report_test_t, leaves_missing_zero_axis_absent)
{
    auto storage = std::array{rel(input_value_t::code_rel_t::x, 17), syn(), input_value_t{}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 2};
    auto report = relative_report_t{frame};

    ASSERT_TRUE(std::move(report).try_store(3, 0));

    EXPECT_TRUE(matches(frame, std::array{rel(input_value_t::code_rel_t::x, 3), syn()}));
}

TEST_F(relative_report_test_t, report_without_xy_motion_remains_without_xy_motion)
{
    auto storage = std::array{syn(), input_value_t{}};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, 1};
    auto report = relative_report_t{frame};

    ASSERT_TRUE(std::move(report).try_store(0, 0));

    EXPECT_TRUE(matches(frame, std::array{syn()}));
}

TEST_F(relative_report_test_t, failed_append_does_not_store_existing_axis)
{
    auto storage = std::array{rel(input_value_t::code_rel_t::x, 17), syn()};
    auto values = input_value_array_adapter_t{storage.data(), storage.size()};
    auto frame = input_frame_t{values, storage.size()};
    auto report = relative_report_t{frame};

    EXPECT_FALSE(std::move(report).try_store(3, 4));

    EXPECT_TRUE(matches(frame, std::array{rel(input_value_t::code_rel_t::x, 17), syn()}));
}

} // namespace
} // namespace crv::pipeline
