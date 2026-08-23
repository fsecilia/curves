// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/pipeline.hpp>
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

    static_assert(std::same_as<spline_t, sut_t::gain_t>);
    static_assert(sizeof(sut_t::config_t) == 64);

    struct constant_gain_t
    {
        auto operator()(float_t) const noexcept -> float_t { return 2.0; }
        auto operator()(jet_t<float_t>) const noexcept -> jet_t<float_t> { return {2.0, 0.0}; }
    };

    struct varying_gain_t
    {
        auto operator()(float_t x) const noexcept -> float_t { return 1.0 + x / 32.0; }
        auto operator()(jet_t<float_t> x) const noexcept -> jet_t<float_t>
        {
            return {1.0 + x.f / 32.0, x.df / 32.0};
        }
    };

    static auto build_gain_spline(auto curve) -> spline_t
    {
        using factory_t
            = spline::spline_factory_t<spline_policy_t, spline::spline_generator_factory_t<spline_policy_t>>;

        auto result = spline_t{};
        factory_t{}(result, spline::gain_curve_target_t{curve}, float_t{2e-6}, std::vector<speed_t>{});
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

    sut_t sut{make_config(), build_gain_spline(constant_gain_t{})};
};

TEST_F(pipeline_integration_test_t, real_components_compose_through_missing_axis_insertion)
{
    auto warmup_storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 1)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const warmup = sut(warmup_storage.data(), 2, warmup_storage.size(), 2, 1'000'000);

    ASSERT_EQ(warmup.status, pipeline::pipeline_result_t::warmup);
    ASSERT_EQ(warmup.count, 2u);
    EXPECT_EQ(load(warmup_storage, 0), rel(input_value_t::code_rel_t::x, 1));

    auto storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const result = sut(storage.data(), 2, storage.size(), 2, 2'000'000);

    ASSERT_EQ(result.status, pipeline::pipeline_result_t::applied);
    ASSERT_EQ(result.count, 2u);
    EXPECT_EQ(load(storage, 0), rel(input_value_t::code_rel_t::y, 12));
    EXPECT_EQ(load(storage, 1), syn());
}

TEST_F(pipeline_integration_test_t, full_raw_frame_reports_append_failure_without_mutation)
{
    auto warmup_storage = std::array{
        abi(rel(input_value_t::code_rel_t::x, 1)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(sut(warmup_storage.data(), 2, warmup_storage.size(), 2, 1'000'000).status,
        pipeline::pipeline_result_t::warmup);

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
    auto active = sut_t{make_config(), build_gain_spline(varying_gain_t{})};
    auto fresh = sut_t{make_config(), build_gain_spline(varying_gain_t{})};

    auto warmup = std::array{
        abi(rel(input_value_t::code_rel_t::x, 1)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(active(warmup.data(), 2, warmup.size(), 2, 1'000'000).status, pipeline::pipeline_result_t::warmup);

    auto history = std::array{
        abi(rel(input_value_t::code_rel_t::x, 13)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    ASSERT_EQ(active(history.data(), 2, history.size(), 2, 2'000'000).status, pipeline::pipeline_result_t::applied);

    auto split = std::array{
        abi(rel(input_value_t::code_rel_t::x, 7)),
        abi(syn()),
        crv_input_value_t{},
        crv_input_value_t{},
    };
    ASSERT_EQ(active(split.data(), 2, split.size(), split.size() - 1, 3'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);

    auto sync_active = std::array{
        abi(rel(input_value_t::code_rel_t::y, 9)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto sync_fresh = sync_active;
    ASSERT_EQ(active(sync_active.data(), 2, sync_active.size(), 2, 4'000'000).status,
        pipeline::pipeline_result_t::split_report_bypassed);
    ASSERT_EQ(fresh(sync_fresh.data(), 2, sync_fresh.size(), 2, 4'000'000).status,
        pipeline::pipeline_result_t::warmup);
    expect_unchanged(sync_active, sync_fresh);

    auto next_active = std::array{
        abi(rel(input_value_t::code_rel_t::x, 3)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto next_fresh = next_active;
    auto const active_result = active(next_active.data(), 2, next_active.size(), 2, 5'000'000);
    auto const fresh_result = fresh(next_fresh.data(), 2, next_fresh.size(), 2, 5'000'000);

    EXPECT_EQ(active_result.status, fresh_result.status);
    EXPECT_EQ(active_result.count, fresh_result.count);
    expect_unchanged(next_active, next_fresh);
}

TEST_F(pipeline_integration_test_t, malformed_forced_callback_enters_desynchronized_state_without_accessing_past_capacity)
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

    auto followup = std::array{
        abi(rel(input_value_t::code_rel_t::x, 2)), abi(syn()), crv_input_value_t{}, crv_input_value_t{}};
    auto const followup_original = followup;
    auto const result = sut(followup.data(), 2, followup.size(), 2, 2'000'000);

    EXPECT_EQ(result.status, pipeline::pipeline_result_t::split_report_bypassed);
    expect_unchanged(followup, followup_original);
}

} // namespace
} // namespace crv
