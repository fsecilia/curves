// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/test/test.hpp>
#include <crv/ui/command/command.hpp>
#include <crv/ui/command/stack.hpp>
#include <string>
#include <thread>
#include <utility>

namespace crv::command {
namespace {

using namespace std::chrono_literals;

struct command_stack_integration_test_t : Test
{
    // simple ui model for commands to mutate
    struct ui_model_t
    {
        int_t param0 = 0;
        int_t param1 = 0;
        std::string text_state = "initial";

        auto operator==(ui_model_t const& other) const -> bool = default;

        friend auto operator<<(std::ostream& out, ui_model_t const& src) -> std::ostream&
        {
            return out << "{param0 = " << src.param0 << ", param1 = " << src.param1 << "}";
        }
    };

    class single_param_command_t : public command_i
    {
    public:
        single_param_command_t(int_t& target, int_t delta, duration_t merge_timeout = 500ms)
            : target_{target}, delta_{delta}, merge_timeout_{merge_timeout}
        {}

        auto redo() noexcept -> void override { target_ += delta_; }
        auto undo() noexcept -> void override { target_ -= delta_; }

        auto merge_timeout() const noexcept -> duration_t override { return merge_timeout_; }

        auto try_merge(command_i&& src) noexcept -> bool override
        {
            auto const* incoming = dynamic_cast<single_param_command_t const*>(&src);
            if (!incoming || &incoming->target_ != &target_) return false;

            delta_ += incoming->delta_;
            return true;
        }

    private:
        int_t& target_;
        int_t delta_;
        duration_t merge_timeout_;
    };

    ui_model_t model;
    using stack_t = stack_t<>;
    stack_t stack;
    stack_t::time_point_t t0 = stack_t::clock_t::now();

    auto undo() -> void
    {
        ASSERT_TRUE(stack.can_undo());
        stack.undo();
    }

    auto redo() -> void
    {
        ASSERT_TRUE(stack.can_redo());
        stack.redo();
    }
};

//
// single member
//

// modifies a single parameter directly by reference
struct command_stack_integration_test_single_param_t : command_stack_integration_test_t
{};

TEST_F(command_stack_integration_test_single_param_t, pair_within_timeout_merges)
{
    // timeout not exceeded, commands are merged into single +15 operation
    stack.emplace<single_param_command_t>(t0, model.param0, 5);
    stack.emplace<single_param_command_t>(t0 + 100ms, model.param0, 10);

    EXPECT_EQ(model.param0, 15);

    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());

    redo();
    EXPECT_EQ(model.param0, 15);
    EXPECT_FALSE(stack.can_redo());
}

TEST_F(command_stack_integration_test_single_param_t, pair_outside_timeout_does_not_merge)
{
    // timeout exceeded, commands should be two separate operations
    stack.emplace<single_param_command_t>(t0, model.param0, 5);
    stack.emplace<single_param_command_t>(t0 + 600ms, model.param0, 10);

    EXPECT_EQ(model.param0, 15);

    undo();
    EXPECT_EQ(model.param0, 5);
    undo();
    EXPECT_EQ(model.param0, 0);

    EXPECT_FALSE(stack.can_undo());

    redo();
    EXPECT_EQ(model.param0, 5);
    redo();
    EXPECT_EQ(model.param0, 15);

    EXPECT_FALSE(stack.can_redo());
}

TEST_F(command_stack_integration_test_single_param_t, pair_undo_redo)
{
    // timeout exceeded, commands should be two separate operations
    stack.emplace<single_param_command_t>(t0, model.param0, 5);
    stack.emplace<single_param_command_t>(t0 + 600ms, model.param0, 10);

    EXPECT_EQ(model.param0, 15);

    undo();
    EXPECT_EQ(model.param0, 5);

    redo();
    EXPECT_EQ(model.param0, 15);
    EXPECT_FALSE(stack.can_redo());

    undo();
    EXPECT_EQ(model.param0, 5);
    undo();
    EXPECT_EQ(model.param0, 0);

    EXPECT_FALSE(stack.can_undo());
}

TEST_F(command_stack_integration_test_single_param_t, timeline_invalidated_on_new_action)
{
    // apply and undo to put event on redo stack
    stack.emplace<single_param_command_t>(t0, model.param0, 10);
    undo();
    EXPECT_TRUE(stack.can_redo());

    // new command clears redo stack
    stack.emplace<single_param_command_t>(t0 + 1s, model.param0, 20);
    EXPECT_FALSE(stack.can_redo());
    EXPECT_EQ(model.param0, 20);
}

TEST_F(command_stack_integration_test_single_param_t, sliding_window_merges_across_total_span)
{
    // each gap stays under 500ms window, but total span exceeds it
    // sliding window keeps whole run collapsed into single event

    stack.emplace<single_param_command_t>(t0, model.param0, 1);
    stack.emplace<single_param_command_t>(t0 + 400ms, model.param0, 2);
    stack.emplace<single_param_command_t>(t0 + 800ms, model.param0, 4);

    EXPECT_EQ(model.param0, 7);

    // 1 merged event: a single undo reverts entire run
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());
}

TEST_F(command_stack_integration_test_single_param_t, drag_run_terminated_by_commit)
{
    // drag: 3 increments inside window collapse into 1 event
    stack.emplace<single_param_command_t>(t0, model.param0, 2);
    stack.emplace<single_param_command_t>(t0 + 100ms, model.param0, 3);
    stack.emplace<single_param_command_t>(t0 + 200ms, model.param0, 5);
    EXPECT_EQ(model.param0, 10);

    // release: 0 timeout makes this instance nonmergeable even though it is same type, same target, and inside window
    stack.emplace<single_param_command_t>(t0 + 250ms, model.param0, 1, 0ms);
    EXPECT_EQ(model.param0, 11);

    // commit is its own event
    undo();
    EXPECT_EQ(model.param0, 10);

    // drag run is a single event
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());
}

TEST_F(command_stack_integration_test_single_param_t, undo_then_action_does_not_merge_into_survivor)
{
    // seed 2 events on different targets so undo leaves one mergeable event on top
    stack.emplace<single_param_command_t>(t0, model.param0, 5);
    stack.emplace<single_param_command_t>(t0 + 100ms, model.param1, 9);

    // undo pops param1 back off; param0 event is now stack top
    undo();
    EXPECT_EQ(model.param1, 0);

    // new action on param0 is same-type, same-target, and inside window, but barrier forces separate event
    stack.emplace<single_param_command_t>(t0 + 200ms, model.param0, 3);
    EXPECT_EQ(model.param0, 8);
    EXPECT_FALSE(stack.can_redo());

    // survivor is intact; undoing new action lands on 5, not 0
    undo();
    EXPECT_EQ(model.param0, 5);
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());
}

TEST_F(command_stack_integration_test_single_param_t, undo_redo_then_action_does_not_merge)
{
    stack.emplace<single_param_command_t>(t0, model.param0, 5);
    undo();
    redo();
    EXPECT_EQ(model.param0, 5);

    // redo restores original timestamp, so without barrier, this action would merge
    stack.emplace<single_param_command_t>(t0 + 100ms, model.param0, 7);
    EXPECT_EQ(model.param0, 12);

    // separate events; undoing new action lands on 5, not 0
    undo();
    EXPECT_EQ(model.param0, 5);
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());
}

//
// mulitple members
//

// modifies multiple members referencing their owner
struct command_stack_integration_test_multiple_params_t : command_stack_integration_test_t
{
    class multi_param_command_t : public command_i
    {
    public:
        multi_param_command_t(ui_model_t& model, int_t delta_0, int_t delta_1)
            : model_{model}, delta_0_{delta_0}, delta_1_{delta_1}
        {}

        auto redo() noexcept -> void override
        {
            model_.param0 += delta_0_;
            model_.param1 += delta_1_;
        }

        auto undo() noexcept -> void override
        {
            model_.param0 -= delta_0_;
            model_.param1 -= delta_1_;
        }

        auto merge_timeout() const noexcept -> duration_t override { return 500ms; }

        auto try_merge(command_i&& src) noexcept -> bool override
        {
            auto const* incoming = dynamic_cast<multi_param_command_t const*>(&src);
            if (!incoming || &incoming->model_ != &model_) return false;

            delta_0_ += incoming->delta_0_;
            delta_1_ += incoming->delta_1_;
            return true;
        }

    private:
        ui_model_t& model_;
        int_t delta_0_;
        int_t delta_1_;
    };
};

TEST_F(command_stack_integration_test_multiple_params_t, pair_within_timeout_merges)
{
    stack.emplace<multi_param_command_t>(t0, model, 1, 2);
    stack.emplace<multi_param_command_t>(t0 + 100ms, model, 3, 4);

    EXPECT_EQ(model.param0, 4);
    EXPECT_EQ(model.param1, 6);

    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_EQ(model.param1, 0);
    EXPECT_FALSE(stack.can_undo());

    redo();
    EXPECT_EQ(model.param0, 4);
    EXPECT_EQ(model.param1, 6);
    EXPECT_FALSE(stack.can_redo());
}

TEST_F(command_stack_integration_test_multiple_params_t, pair_outside_timeout_does_not_merge)
{
    stack.emplace<multi_param_command_t>(t0, model, 1, 2);
    stack.emplace<multi_param_command_t>(t0 + 600ms, model, 3, 4);

    EXPECT_EQ(model.param0, 4);
    EXPECT_EQ(model.param1, 6);

    undo();
    EXPECT_EQ(model.param0, 1);
    EXPECT_EQ(model.param1, 2);
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_EQ(model.param1, 0);

    EXPECT_FALSE(stack.can_undo());

    redo();
    EXPECT_EQ(model.param0, 1);
    EXPECT_EQ(model.param1, 2);
    redo();
    EXPECT_EQ(model.param0, 4);
    EXPECT_EQ(model.param1, 6);

    EXPECT_FALSE(stack.can_redo());
}

TEST_F(command_stack_integration_test_multiple_params_t, pair_undo_redo)
{
    stack.emplace<multi_param_command_t>(t0, model, 1, 2);
    stack.emplace<multi_param_command_t>(t0 + 600ms, model, 3, 4);

    EXPECT_EQ(model.param0, 4);
    EXPECT_EQ(model.param1, 6);

    undo();
    EXPECT_EQ(model.param0, 1);
    EXPECT_EQ(model.param1, 2);

    redo();
    EXPECT_EQ(model.param0, 4);
    EXPECT_EQ(model.param1, 6);
    EXPECT_FALSE(stack.can_redo());

    undo();
    EXPECT_EQ(model.param0, 1);
    EXPECT_EQ(model.param1, 2);
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_EQ(model.param1, 0);

    EXPECT_FALSE(stack.can_undo());
}

//
// whole object snapshot
//

// snapshots whole object and replaces it entirely
struct command_stack_integration_test_snapshot_t : command_stack_integration_test_t
{
    // modifies a whole model all at once, snapshotting current state when created, applies whole saved state
    class snapshot_command_t : public nonmergeable_command_t
    {
    public:
        snapshot_command_t(ui_model_t& model, ui_model_t const& state_redo)
            : model_{model}, state_undo_{model}, state_redo_{state_redo}
        {}

        auto redo() -> void override
        {
            // make local copy and invoke move ctor to guarantee exception safety
            auto next = state_redo_;
            model_ = std::move(next);
        }

        auto undo() -> void override
        {
            // make local copy and invoke move ctor to guarantee exception safety
            auto prev = state_undo_;
            model_ = std::move(prev);
        }

    private:
        ui_model_t& model_;
        ui_model_t state_undo_;
        ui_model_t state_redo_;
    };
};

TEST_F(command_stack_integration_test_snapshot_t, full_snapshot)
{
    model.text_state = "undo_state";
    model.param0 = 37;

    // create target memento state
    auto target_state = model;
    target_state.text_state = "redo_state";
    target_state.param1 = 99;

    // push snapshot
    stack.emplace<snapshot_command_t>(t0, model, target_state);
    EXPECT_EQ(model.text_state, "redo_state");
    EXPECT_EQ(model.param0, 37);
    EXPECT_EQ(model.param1, 99);

    // revert back to working state
    undo();
    EXPECT_EQ(model.text_state, "undo_state");
    EXPECT_EQ(model.param0, 37);
    EXPECT_EQ(model.param1, 0);

    // redo applied state
    redo();
    EXPECT_EQ(model.text_state, "redo_state");
    EXPECT_EQ(model.param0, 37);
    EXPECT_EQ(model.param1, 99);
}

TEST_F(command_stack_integration_test_snapshot_t, mixed_command_types_round_trip)
{
    // a delta, then a nonmergeable snapshot, then another delta, is always 3 distinct events
    stack.emplace<single_param_command_t>(t0, model.param0, 5);

    auto target = model;
    target.param1 = 37;
    target.text_state = "text_state";
    stack.emplace<snapshot_command_t>(t0 + 100ms, model, target);

    stack.emplace<single_param_command_t>(t0 + 200ms, model.param0, 3);

    EXPECT_EQ(model, (ui_model_t{.param0 = 8, .param1 = 37, .text_state = "text_state"}));

    // undo whole chain back to default state
    undo();
    undo();
    undo();
    EXPECT_EQ(model, ui_model_t{});
    EXPECT_FALSE(stack.can_undo());

    // redo whole chain forward again
    redo();
    redo();
    redo();
    EXPECT_EQ(model, (ui_model_t{.param0 = 8, .param1 = 37, .text_state = "text_state"}));
    EXPECT_FALSE(stack.can_redo());
}

TEST_F(command_stack_integration_test_snapshot_t, delta_then_snapshot_does_not_merge)
{
    // delta on top, snapshot incoming, inside window
    // snapshot's 0 timeout vetoes via min
    stack.emplace<single_param_command_t>(t0, model.param0, 5);

    auto target = model;
    target.param1 = 9;
    stack.emplace<snapshot_command_t>(t0 + 100ms, model, target);

    EXPECT_EQ(model.param0, 5);
    EXPECT_EQ(model.param1, 9);

    // 2 events: undoing snapshot leaves param0 untouched at 5
    undo();
    EXPECT_EQ(model.param0, 5);
    EXPECT_EQ(model.param1, 0);
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());
}

TEST_F(command_stack_integration_test_snapshot_t, snapshot_then_delta_does_not_merge)
{
    // snapshot on top, delta incoming, inside window
    // snapshot sits on stack and its 0 timeout vetos incoming delta via min(0, 500ms)
    auto target = model;
    target.param1 = 9;
    stack.emplace<snapshot_command_t>(t0, model, target);

    stack.emplace<single_param_command_t>(t0 + 100ms, model.param0, 5);

    EXPECT_EQ(model.param0, 5);
    EXPECT_EQ(model.param1, 9);

    // 2 events: undoing delta leaves snapshot's param1 in place
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_EQ(model.param1, 9);
    undo();
    EXPECT_EQ(model.param1, 0);
    EXPECT_FALSE(stack.can_undo());
}

//
// live timeout
//

// uses (minimal) live thread sleeps to test expiring timeouts
struct command_stack_integration_test_live_timeout_t : command_stack_integration_test_t
{
    class timeout_command_t : public command_i
    {
    public:
        timeout_command_t(int_t& target) : target_{target} {}
        auto redo() noexcept -> void override { target_ += 1; }
        auto undo() noexcept -> void override { target_ -= 1; }

        auto merge_timeout() const noexcept -> duration_t override { return 5ms; }

        // increments by 10 when merging
        auto try_merge(command_i&& src) noexcept -> bool override
        {
            if (!dynamic_cast<timeout_command_t const*>(&src)) return false;
            target_ += 10;
            return true;
        }

    private:
        int_t& target_;
    };
};

// pushes commands using default timeout param clock_t::now()
TEST_F(command_stack_integration_test_live_timeout_t, default_push)
{
    auto command1 = std::make_unique<timeout_command_t>(model.param0);
    stack.push(std::move(command1));

    // force timeout expiration
    std::this_thread::sleep_for(10ms);

    auto command2 = std::make_unique<timeout_command_t>(model.param0);
    stack.push(std::move(command2));

    EXPECT_EQ(model.param0, 2);

    // sleep forces a timeout, so try_merge must fail - result is two events
    undo();
    EXPECT_EQ(model.param0, 1);
    EXPECT_TRUE(stack.can_undo());
}

// uses emplace_now to use clock_t::now()
TEST_F(command_stack_integration_test_live_timeout_t, emplace_now)
{
    stack.emplace_now<single_param_command_t>(model.param0, 3);
    stack.emplace_now<single_param_command_t>(model.param0, 5);

    EXPECT_EQ(model.param0, 8);

    // since timeout does not expire, try_merge must succeed
    undo();
    EXPECT_EQ(model.param0, 0);
    EXPECT_FALSE(stack.can_undo());
}

} // namespace
} // namespace crv::command
