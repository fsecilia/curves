// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "mutate_param.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::command {
namespace {

struct command_mutate_param_test_t : Test
{
    using value_t = int_t;
    static constexpr auto initial_param_value = 3;
    static constexpr auto mutated_param_value = 5;
    static constexpr auto merged_param_value = 7;

    struct param_t
    {
        using value_t = value_t;

        value_t value_;

        constexpr auto value() const noexcept -> value_t { return value_; }
        constexpr auto value(value_t value) noexcept -> bool
        {
            auto const result = value != value_;
            value_ = value;
            return result;
        }
    };
    param_t param_{initial_param_value};

    struct mock_notification_target_t
    {
        virtual ~mock_notification_target_t() = default;

        MOCK_METHOD(void, on_changed, (param_t & changed_param, value_t prev_value), (const, noexcept));
    };
    StrictMock<mock_notification_target_t> mock_notification_target;

    using sut_t = mutate_param_t<param_t>;
    sut_t sut{param_, mutated_param_value, [&](param_t& changed_param, value_t prev_value) {
                  mock_notification_target.on_changed(changed_param, prev_value);
              }};
};

TEST_F(command_mutate_param_test_t, id)
{
    EXPECT_EQ(id_t::mutate_param, sut.id());
}

TEST_F(command_mutate_param_test_t, apply)
{
    EXPECT_CALL(mock_notification_target, on_changed(Ref(param_), initial_param_value));

    sut.apply();

    // apply updates value to next
    EXPECT_EQ(mutated_param_value, param_.value());
}

TEST_F(command_mutate_param_test_t, undo)
{
    EXPECT_CALL(mock_notification_target, on_changed(Ref(param_), mutated_param_value));
    param_.value(mutated_param_value);

    sut.undo();

    // undo restores initial value
    EXPECT_EQ(initial_param_value, param_.value());
}

TEST_F(command_mutate_param_test_t, idle_duration)
{
    EXPECT_EQ(sut_t::idle_duration_ms, std::chrono::duration_cast<std::chrono::milliseconds>(sut.idle_duration()));
}

TEST_F(command_mutate_param_test_t, try_merge_self_succeeds_but_does_nothing)
{
    EXPECT_TRUE(sut.try_merge(sut));

    // apply still applies original next
    EXPECT_CALL(mock_notification_target, on_changed(Ref(param_), initial_param_value));
    sut.apply();
    EXPECT_EQ(mutated_param_value, param_.value());
}

TEST_F(command_mutate_param_test_t, try_merge_other_different_param_fails)
{
    auto other_param = param_t{};
    auto const other_sut = sut_t{other_param, merged_param_value, [&](param_t& changed_param, value_t prev_value) {
                                     mock_notification_target.on_changed(changed_param, prev_value);
                                 }};

    // other params are bounced
    EXPECT_FALSE(sut.try_merge(other_sut));
}

TEST_F(command_mutate_param_test_t, try_merge_other_same_param_succeeds)
{
    auto const other_sut = sut_t{param_, merged_param_value, [&](param_t& changed_param, value_t prev_value) {
                                     mock_notification_target.on_changed(changed_param, prev_value);
                                 }};

    EXPECT_TRUE(sut.try_merge(other_sut));

    // merged param value is applied instead of mutated
    EXPECT_CALL(mock_notification_target, on_changed(Ref(param_), initial_param_value));
    sut.apply();
    EXPECT_EQ(merged_param_value, param_.value());
}

TEST_F(command_mutate_param_test_t, try_merge_preserves_undo_state)
{
    auto const other_sut = sut_t{param_, merged_param_value, [&](param_t& changed_param, value_t prev_value) {
                                     mock_notification_target.on_changed(changed_param, prev_value);
                                 }};
    param_.value(mutated_param_value);

    EXPECT_TRUE(sut.try_merge(other_sut));

    // initial_param_value is still restored even after successful merge
    EXPECT_CALL(mock_notification_target, on_changed(testing::Ref(param_), mutated_param_value));
    sut.undo();
    EXPECT_EQ(initial_param_value, param_.value());
}

} // namespace
} // namespace crv::command
