// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "committer.hpp"
#include <crv/test/test.hpp>
#include <tuple>
#include <utility>
#include <vector>

namespace crv::pipeline::configuration {
namespace {

struct committer_test_t : Test
{
    struct config_t
    {
        int_t value{};
        constexpr auto operator==(config_t const&) const noexcept -> bool = default;
    };

    struct gain_t
    {
        int_t value{};
        constexpr auto operator==(gain_t const&) const noexcept -> bool = default;
    };

    struct state_t
    {
        int_t value{};
        constexpr auto operator==(state_t const&) const noexcept -> bool = default;
    };

    enum class mode_t : uint8_t
    {
        unconfigured,
        bypassed,
        active,
    };

    struct target_t
    {
        config_t config{.value = 1};
        gain_t gain{.value = 2};
        state_t state{.value = 3};
        bool synchronized = false;
        mode_t mode = mode_t::unconfigured;

        template <typename operation_t> auto commit_configuration(operation_t&& operation) noexcept -> void
        {
            std::forward<operation_t>(operation)(config, gain, state, mode);
        }
    };

    struct candidate_t
    {
        config_t config{.value = 11};
        gain_t gain{.value = 22};
        apply_mode_t mode = apply_mode_t::active;
    };

    struct validated_candidate_t
    {
        candidate_t const& candidate;
    };

    candidate_t candidate{};
    validated_candidate_t validated{candidate};
    target_t target{};
    committer_t sut{};

    auto commit() noexcept -> void { sut(target, validated); }
};

TEST_F(committer_test_t, replaces_config)
{
    commit();
    EXPECT_EQ(target.config, candidate.config);
}

TEST_F(committer_test_t, replaces_gain)
{
    commit();
    EXPECT_EQ(target.gain, candidate.gain);
}

TEST_F(committer_test_t, resets_numerical_state)
{
    commit();
    EXPECT_EQ(target.state, state_t{});
}

TEST_F(committer_test_t, preserves_synchronization)
{
    commit();
    EXPECT_FALSE(target.synchronized);
}

using live_mode_t = committer_test_t::mode_t;

struct committer_mode_test_t : committer_test_t, WithParamInterface<std::tuple<live_mode_t, apply_mode_t, live_mode_t>>
{};

TEST_P(committer_mode_test_t, maps_apply_mode_independently_of_previous_live_mode)
{
    auto const [previous_mode, apply_mode, expected_mode] = GetParam();
    target.mode = previous_mode;
    candidate.mode = apply_mode;

    commit();

    EXPECT_EQ(target.mode, expected_mode);
}

INSTANTIATE_TEST_SUITE_P(apply_mode_transitions, committer_mode_test_t,
    Values(std::tuple{live_mode_t::unconfigured, apply_mode_t::bypassed, live_mode_t::bypassed},
        std::tuple{live_mode_t::unconfigured, apply_mode_t::active, live_mode_t::active},
        std::tuple{live_mode_t::bypassed, apply_mode_t::bypassed, live_mode_t::bypassed},
        std::tuple{live_mode_t::bypassed, apply_mode_t::active, live_mode_t::active},
        std::tuple{live_mode_t::active, apply_mode_t::bypassed, live_mode_t::bypassed},
        std::tuple{live_mode_t::active, apply_mode_t::active, live_mode_t::active}));

struct assignment_order_test_t : Test
{
    struct trace_t
    {
        std::vector<int_t> assignments;
    };

    template <int_t id> struct tracked_t
    {
        trace_t* trace = nullptr;
        int_t value{};

        auto operator=(tracked_t const& other) noexcept -> tracked_t&
        {
            if (trace) trace->assignments.push_back(id);
            value = other.value;
            return *this;
        }
    };

    struct mode_t
    {
        enum class value_t : uint8_t
        {
            unconfigured,
            bypassed,
            active,
        };

        static mode_t const bypassed;
        static mode_t const active;

        trace_t* trace = nullptr;
        value_t value = value_t::unconfigured;

        constexpr mode_t() noexcept = default;
        constexpr mode_t(mode_t const&) noexcept = default;
        constexpr explicit mode_t(value_t value) noexcept : value{value} {}
        constexpr mode_t(trace_t* trace, value_t value) noexcept : trace{trace}, value{value} {}

        auto operator=(mode_t const& other) noexcept -> mode_t&
        {
            if (trace) trace->assignments.push_back(4);
            value = other.value;
            return *this;
        }
    };

    struct target_t
    {
        trace_t trace{};
        tracked_t<1> config{&trace, 1};
        tracked_t<2> gain{&trace, 2};
        tracked_t<3> state{&trace, 3};
        mode_t mode{&trace, mode_t::value_t::unconfigured};

        template <typename operation_t> auto commit_configuration(operation_t&& operation) noexcept -> void
        {
            std::forward<operation_t>(operation)(config, gain, state, mode);
        }
    };

    struct candidate_t
    {
        tracked_t<1> config{nullptr, 11};
        tracked_t<2> gain{nullptr, 22};
        apply_mode_t mode = apply_mode_t::active;
    };

    struct validated_candidate_t
    {
        candidate_t const& candidate;
    };
};

assignment_order_test_t::mode_t const assignment_order_test_t::mode_t::bypassed{mode_t::value_t::bypassed};
assignment_order_test_t::mode_t const assignment_order_test_t::mode_t::active{mode_t::value_t::active};

TEST_F(assignment_order_test_t, writes_mode_last)
{
    auto const candidate = candidate_t{};
    auto const validated = validated_candidate_t{candidate};
    auto target = target_t{};

    committer_t{}(target, validated);

    EXPECT_EQ(target.trace.assignments, (std::vector<int_t>{1, 2, 3, 4}));
}

} // namespace
} // namespace crv::pipeline::configuration
