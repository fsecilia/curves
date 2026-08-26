// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/pipeline.hpp>
#include <crv/pipeline/configuration/candidate.hpp>
#include <crv/pipeline/configuration/committer.hpp>
#include <crv/pipeline/configuration/transaction.hpp>
#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <concepts>
#include <vector>

namespace crv {
namespace {

struct pipeline_integration_test_t : Test
{
    using sut_t = pipeline_t;
    using spline_policy_t = spline::default_spline_policy_t<float_t, spline::prod_pipeline_config_t>;
    using spline_t = spline_policy_t::spline_t;
    using speed_t = spline_policy_t::x_t;
    using duration_t = sut_t::duration_t;
    using output_transform_t = pipeline::output_transform_t<spline_policy_t::y_t>;
    using configuration_transaction_t
        = pipeline::configuration::transaction_t<sut_t::validator_t, pipeline::configuration::committer_t>;

    static_assert(std::same_as<spline_t, sut_t::gain_t>);
    static_assert(sizeof(sut_t::config_t) == 48);
    static_assert(sizeof(sut_t::mode_t) == 1);

    struct constant_gain_t
    {
        auto operator()(float_t) const noexcept -> float_t { return 2.0; }
        auto operator()(jet_t<float_t>) const noexcept -> jet_t<float_t> { return {2.0, 0.0}; }
    };

    struct varying_gain_t
    {
        auto operator()(float_t x) const noexcept -> float_t { return 1.0 + x / 32.0; }
        auto operator()(jet_t<float_t> x) const noexcept -> jet_t<float_t> { return {1.0 + x.f / 32.0, x.df / 32.0}; }
    };

    static auto build_gain_spline(auto curve) -> spline_t
    {
        using factory_t
            = spline::spline_factory_t<spline_policy_t, spline::spline_generator_factory_t<spline_policy_t>>;

        auto result = spline_t{};
        factory_t{}(
            result, spline::gain_curve_target_t{curve}, spline_policy_t::spline_gain_tolerance, std::vector<speed_t>{});
        return result;
    }

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

    static constexpr auto abi(input_value_t const& value) noexcept -> crv_input_value_t
    {
        return {
            .type = static_cast<crv_u16_t>(value.type),
            .code = value.code,
            .value = value.value,
        };
    }

    template <std::size_t size>
    static auto load(std::array<crv_input_value_t, size>& storage, std::size_t index) noexcept -> input_value_t
    {
        return input_value_array_adapter_t{storage.data(), storage.size()}.load(index);
    }

    template <std::size_t size>
    static auto expect_unchanged(
        std::array<crv_input_value_t, size> const& actual, std::array<crv_input_value_t, size> const& expected) -> void
    {
        EXPECT_EQ(0, __builtin_memcmp(actual.data(), expected.data(), sizeof(actual)));
    }

    static auto make_config() -> sut_t::config_t
    {
        return {
            .velocity_scale = sut_t::velocity_scale_t{1'000'000},
            .half_life = duration_t{1'500'000},
            .output_transform
            = output_transform_t{.matrix = {{
                                     {output_transform_t::coefficient_t{}, -output_transform_t::coefficient_t{1}},
                                     {output_transform_t::coefficient_t{2}, output_transform_t::coefficient_t{}},
                                 }}},
        };
    }

    static auto make_candidate(pipeline::configuration::apply_mode_t mode, auto curve)
        -> pipeline::configuration::candidate_t
    {
        return {
            .config = make_config(),
            .mode = mode,
            .gain = build_gain_spline(curve),
        };
    }

    static auto make_candidate(pipeline::configuration::apply_mode_t mode) -> pipeline::configuration::candidate_t
    {
        return make_candidate(mode, varying_gain_t{});
    }

    auto apply(sut_t& target, pipeline::configuration::apply_mode_t mode, auto curve) -> void
    {
        auto const candidate = make_candidate(mode, curve);
        auto const validated = transaction.validate(candidate);
        ASSERT_TRUE(validated);
        transaction.commit(target, *validated);
    }

    auto apply(sut_t& target, pipeline::configuration::apply_mode_t mode) -> void
    {
        apply(target, mode, varying_gain_t{});
    }

    static auto timer_initialized(sut_t& target) noexcept -> bool
    {
        auto initialized = false;
        target.commit_configuration(
            [&](auto&, auto&, auto& state, auto&) noexcept { initialized = state.timer.initialized(); });
        return initialized;
    }

    auto SetUp() -> void override { apply(sut, pipeline::configuration::apply_mode_t::active, constant_gain_t{}); }

    configuration_transaction_t transaction{};
    sut_t sut{};
};

TEST_F(pipeline_integration_test_t, committed_active_candidate_sets_active_mode)
{
    auto const candidate = make_candidate(pipeline::configuration::apply_mode_t::active);
    auto const validated = transaction.validate(candidate).value();

    transaction.commit(sut, validated);

    EXPECT_EQ(sut.mode(), sut_t::mode_t::active);
}

TEST_F(pipeline_integration_test_t, committed_bypassed_candidate_sets_bypassed_mode)
{
    auto const candidate = make_candidate(pipeline::configuration::apply_mode_t::bypassed);
    auto const validated = transaction.validate(candidate).value();

    transaction.commit(sut, validated);

    EXPECT_EQ(sut.mode(), sut_t::mode_t::bypassed);
}

TEST_F(pipeline_integration_test_t, configuration_commit_preserves_desynchronized_state)
{
    auto split = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    (void)sut(split.data(), 2, split.size(), split.size() - 1, 1'000'000);

    auto const candidate = make_candidate(pipeline::configuration::apply_mode_t::bypassed);
    auto const validated = transaction.validate(candidate).value();
    transaction.commit(sut, validated);

    EXPECT_FALSE(sut.synchronized());
}

TEST_F(pipeline_integration_test_t, first_report_after_configuration_commit_uses_warmup_path)
{
    auto initial = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    (void)sut(initial.data(), 2, initial.size(), 2, 1'000'000);

    auto const candidate = make_candidate(pipeline::configuration::apply_mode_t::active);
    auto const validated = transaction.validate(candidate).value();
    transaction.commit(sut, validated);

    auto next = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };

    EXPECT_EQ(sut(next.data(), 2, next.size(), 2, 2'000'000).status, pipeline::pipeline_result_t::warmup);
}

TEST_F(pipeline_integration_test_t, defaults_to_unconfigured_and_synchronized)
{
    auto const unconfigured = sut_t{};

    EXPECT_EQ(unconfigured.mode(), sut_t::mode_t::unconfigured);
    EXPECT_TRUE(unconfigured.synchronized());
}

TEST_F(pipeline_integration_test_t, generated_runtime_candidate_passes_shared_validation)
{
    EXPECT_TRUE(sut_t::validate(make_config(), build_gain_spline(varying_gain_t{})));
}

TEST_F(pipeline_integration_test_t, activation_mode_is_independent_of_report_synchronization)
{
    auto unconfigured = sut_t{};
    auto split = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    ASSERT_EQ(unconfigured(split.data(), 2, split.size(), split.size() - 1, 1'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_EQ(unconfigured.mode(), sut_t::mode_t::unconfigured);
    EXPECT_FALSE(unconfigured.synchronized());

    auto followup
        = std::array{abi(rel(input_value_t::code_rel_t::x, 2)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(unconfigured(followup.data(), 2, followup.size(), 2, 2'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_EQ(unconfigured.mode(), sut_t::mode_t::unconfigured);
    EXPECT_TRUE(unconfigured.synchronized());
}

TEST_F(pipeline_integration_test_t, unconfigured_report_is_inactive_and_leaves_timer_dormant)
{
    auto unconfigured = sut_t{};
    auto storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const original = storage;

    auto const result = unconfigured(storage.data(), 2, storage.size(), 2, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::inactive);
    EXPECT_FALSE(timer_initialized(unconfigured));
    expect_unchanged(storage, original);
}

TEST_F(pipeline_integration_test_t, bypassed_report_is_inactive_and_leaves_timer_dormant)
{
    auto bypassed = sut_t{};
    apply(bypassed, pipeline::configuration::apply_mode_t::bypassed, constant_gain_t{});
    auto storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const original = storage;

    auto const result = bypassed(storage.data(), 2, storage.size(), 2, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::inactive);
    EXPECT_FALSE(timer_initialized(bypassed));
    expect_unchanged(storage, original);
}

TEST_F(pipeline_integration_test_t, forced_split_while_unconfigured_desynchronizes_without_advancing_timer)
{
    auto unconfigured = sut_t{};
    auto storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};

    auto const result = unconfigured(storage.data(), 2, storage.size(), storage.size() - 1, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_FALSE(unconfigured.synchronized());
    EXPECT_FALSE(timer_initialized(unconfigured));
}

TEST_F(pipeline_integration_test_t, forced_split_while_bypassed_desynchronizes_without_advancing_timer)
{
    auto bypassed = sut_t{};
    apply(bypassed, pipeline::configuration::apply_mode_t::bypassed, constant_gain_t{});
    auto storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};

    auto const result = bypassed(storage.data(), 2, storage.size(), storage.size() - 1, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_FALSE(bypassed.synchronized());
    EXPECT_FALSE(timer_initialized(bypassed));
}

TEST_F(pipeline_integration_test_t, inactive_recovery_restores_framing_without_advancing_timer)
{
    auto bypassed = sut_t{};
    apply(bypassed, pipeline::configuration::apply_mode_t::bypassed, constant_gain_t{});
    auto split
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(bypassed(split.data(), 2, split.size(), split.size() - 1, 1'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);

    auto recovery
        = std::array{abi(rel(input_value_t::code_rel_t::y, 4)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const result = bypassed(recovery.data(), 2, recovery.size(), 2, 2'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_TRUE(bypassed.synchronized());
    EXPECT_FALSE(timer_initialized(bypassed));
}

TEST_F(pipeline_integration_test_t, apply_active_during_split_recovery_preserves_framing_epoch)
{
    auto pipeline = sut_t{};
    apply(pipeline, pipeline::configuration::apply_mode_t::bypassed, constant_gain_t{});
    auto split
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(pipeline(split.data(), 2, split.size(), split.size() - 1, 1'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);

    apply(pipeline, pipeline::configuration::apply_mode_t::active, constant_gain_t{});

    auto recovery
        = std::array{abi(rel(input_value_t::code_rel_t::x, 5)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(pipeline(recovery.data(), 2, recovery.size(), 2, 2'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);

    auto next
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};

    EXPECT_EQ(pipeline(next.data(), 2, next.size(), 2, 3'000'000).status, pipeline::pipeline_result_t::applied);
}

TEST_F(pipeline_integration_test_t, real_components_compose_through_missing_axis_insertion)
{
    auto warmup_storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 1)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const warmup = sut(warmup_storage.data(), 2, warmup_storage.size(), 2, 1'000'000);

    ASSERT_EQ(warmup.status, pipeline::pipeline_result_t::warmup);
    ASSERT_EQ(warmup.count, 2u);
    EXPECT_EQ(load(warmup_storage, 0), rel(input_value_t::code_rel_t::x, 1));

    auto storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const result = sut(storage.data(), 2, storage.size(), 2, 2'000'000);

    ASSERT_EQ(result.status, pipeline::pipeline_result_t::applied);
    ASSERT_EQ(result.count, 2u);
    EXPECT_EQ(load(storage, 0), rel(input_value_t::code_rel_t::y, 12));
    EXPECT_EQ(load(storage, 1), syn());
}

TEST_F(pipeline_integration_test_t, full_raw_frame_reports_append_failure_without_mutation)
{
    auto warmup_storage
        = std::array{abi(rel(input_value_t::code_rel_t::x, 1)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(
        sut(warmup_storage.data(), 2, warmup_storage.size(), 2, 1'000'000).status, pipeline::pipeline_result_t::warmup);

    auto storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        crv_input_value_t{.type = 1, .code = 0, .value = 1},
        crv_input_value_t{.type = 1, .code = 0, .value = 2},
        abi(syn()),
    };
    auto const original = storage;
    auto const result = sut(storage.data(), storage.size(), storage.size(), 2, 2'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::append_failed);
    EXPECT_EQ(result.count, storage.size());
    expect_unchanged(storage, original);
}

TEST_F(pipeline_integration_test_t, invalid_raw_frame_returns_original_count_without_accessing_past_capacity)
{
    auto storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    auto const original = storage;

    auto const result = sut(storage.data(), storage.size() + 1, storage.size(), 2, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::invalid_report);
    EXPECT_EQ(result.count, storage.size() + 1);
    expect_unchanged(storage, original);
}

TEST_F(pipeline_integration_test_t, full_incoming_callback_is_not_a_forced_split_when_num_vals_is_small)
{
    auto storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        crv_input_value_t{.type = 1, .code = 0, .value = 1},
        abi(rel(input_value_t::code_rel_t::y, 4)),
        abi(syn()),
    };
    auto const original = storage;

    auto const result = sut(storage.data(), storage.size(), storage.size(), 2, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::warmup);
    EXPECT_EQ(result.count, storage.size());
    expect_unchanged(storage, original);
}

TEST_F(pipeline_integration_test_t, forced_split_is_detected_when_incoming_count_was_reduced)
{
    auto storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    auto const original = storage;

    auto const result = sut(storage.data(), 2, storage.size(), storage.size() - 1, 1'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_EQ(result.count, 2u);
    expect_unchanged(storage, original);
}

TEST_F(pipeline_integration_test_t, split_callbacks_and_first_complete_followup_are_bypassed_unchanged)
{
    auto first = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    auto const first_original = first;
    auto const first_result = sut(first.data(), 2, first.size(), first.size() - 1, 1'000'000);
    ASSERT_EQ(first_result.status, pipeline::pipeline_result_t::split_report_bypassed);
    expect_unchanged(first, first_original);

    auto second = std::array{
        abi(rel(input_value_t::code_rel_t::y, 4)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    auto const second_original = second;
    auto const second_result = sut(second.data(), 2, second.size(), second.size() - 1, 2'000'000);
    ASSERT_EQ(second_result.status, pipeline::pipeline_result_t::split_report_bypassed);
    expect_unchanged(second, second_original);

    auto followup = std::array{
        abi(rel(input_value_t::code_rel_t::x, 5)),
        abi(rel(input_value_t::code_rel_t::y, 6)),
        abi(syn()),
        crv_input_value_t{},
    };
    auto const followup_original = followup;
    auto const followup_result = sut(followup.data(), 3, followup.size(), 3, 3'000'000);

    EXPECT_EQ(followup_result.status, pipeline::pipeline_result_t::split_report_bypassed);
    EXPECT_EQ(followup_result.count, 3u);
    expect_unchanged(followup, followup_original);
}

TEST_F(pipeline_integration_test_t, first_report_after_resynchronization_uses_a_fresh_runtime_state)
{
    auto active = sut_t{};
    auto fresh = sut_t{};
    apply(active, pipeline::configuration::apply_mode_t::active);
    apply(fresh, pipeline::configuration::apply_mode_t::active);

    auto warmup
        = std::array{abi(rel(input_value_t::code_rel_t::x, 1)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(active(warmup.data(), 2, warmup.size(), 2, 1'000'000).status, pipeline::pipeline_result_t::warmup);

    auto history
        = std::array{abi(rel(input_value_t::code_rel_t::x, 13)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(active(history.data(), 2, history.size(), 2, 2'000'000).status, pipeline::pipeline_result_t::applied);

    auto split = std::array{
        abi(rel(input_value_t::code_rel_t::x, 7)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    ASSERT_EQ(active(split.data(), 2, split.size(), split.size() - 1, 3'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);

    auto sync_active
        = std::array{abi(rel(input_value_t::code_rel_t::y, 9)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto sync_fresh = sync_active;
    ASSERT_EQ(active(sync_active.data(), 2, sync_active.size(), 2, 4'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);
    ASSERT_EQ(fresh(sync_fresh.data(), 2, sync_fresh.size(), 2, 4'000'000).status, pipeline::pipeline_result_t::warmup);
    expect_unchanged(sync_active, sync_fresh);

    auto next_active
        = std::array{abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto next_fresh = next_active;
    auto const active_result = active(next_active.data(), 2, next_active.size(), 2, 5'000'000);
    auto const fresh_result = fresh(next_fresh.data(), 2, next_fresh.size(), 2, 5'000'000);

    EXPECT_EQ(active_result.status, fresh_result.status);
    EXPECT_EQ(active_result.count, fresh_result.count);
    expect_unchanged(next_active, next_fresh);
}

TEST_F(
    pipeline_integration_test_t, malformed_forced_callback_enters_desynchronized_state_without_accessing_past_capacity)
{
    auto storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    auto const original = storage;

    auto const malformed = sut(storage.data(), storage.size() + 1, storage.size(), storage.size() - 1, 1'000'000);
    EXPECT_EQ(malformed.status, pipeline::pipeline_result_t::invalid_report);
    EXPECT_EQ(malformed.count, storage.size() + 1);
    expect_unchanged(storage, original);

    auto followup
        = std::array{abi(rel(input_value_t::code_rel_t::x, 2)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const followup_original = followup;
    auto const result = sut(followup.data(), 2, followup.size(), 2, 2'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::split_report_bypassed);
    expect_unchanged(followup, followup_original);
}

} // namespace
} // namespace crv
