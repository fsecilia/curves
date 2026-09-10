// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline_generator.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/spline/construction/error.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

struct spline_generator_test_t : Test
{
    struct target_t
    {
        constexpr auto transfer(float_t x) const noexcept -> float_t { return x; }
    };
    static constexpr auto target = target_t{};
    using scalar_t = float_t;

    using x_t = fixed_t<int_t, 0>;
    using error_t = spline_construction_error_t<x_t>;

    using critical_points_t = std::vector<x_t>;

    struct spline_t
    {
        int_t id = 7;
    };

    using refinement_pool_t = int_t;

    bool workspace_empty = true;
    struct workspace_t
    {
        bool empty() const { return *empty_; }
        void clear() { *empty_ = true; }
        bool* empty_ = nullptr;
    };

    struct initial_state_t
    {
        workspace_t& ws;
    };

    struct unrefined_state_t
    {
        workspace_t& workspace;
        int_t id = 0;
    };

    struct unassembled_state_t
    {
        workspace_t& workspace;
        int_t id = 0;
    };

    struct typestates_t
    {
        using workspace_t = workspace_t;
        using initial_t = initial_state_t;
        using unassembled_t = unassembled_state_t;
    };

    using result_t = std::expected<void, error_t>;
    using seeding_result_t = std::expected<unrefined_state_t, error_t>;
    using refinement_result_t = std::expected<unassembled_state_t, error_t>;
    using assembly_result_t = std::expected<void, error_t>;

    struct mock_refinement_seeder_t
    {
        virtual ~mock_refinement_seeder_t() = default;
        MOCK_METHOD(seeding_result_t, call, (initial_state_t, target_t const&, critical_points_t));
    };
    StrictMock<mock_refinement_seeder_t> mock_seeder;

    struct refinement_seeder_t
    {
        using critical_points_t = spline_generator_test_t::critical_points_t;
        using error_t = spline_generator_test_t::error_t;
        mock_refinement_seeder_t* mock;

        auto operator()(initial_state_t state, auto const& passed_target, critical_points_t const& critical_points)
            -> seeding_result_t
        {
            return mock->call(state, passed_target, critical_points);
        }
    };

    struct mock_refiner_t
    {
        virtual ~mock_refiner_t() = default;
        MOCK_METHOD(refinement_result_t, call, (unrefined_state_t, target_t const&));
    };
    StrictMock<mock_refiner_t> mock_refiner;

    struct refiner_t
    {
        using error_t = spline_generator_test_t::error_t;
        using result_t = spline_generator_test_t::refinement_result_t;

        mock_refiner_t* mock;
        auto operator()(unrefined_state_t state, auto const& passed_target) { return mock->call(state, passed_target); }
    };

    struct mock_assembler_t
    {
        virtual ~mock_assembler_t() = default;
        MOCK_METHOD(assembly_result_t, call, (unassembled_state_t, spline_t&));
    };
    StrictMock<mock_assembler_t> mock_assembler;

    struct assembler_t
    {
        using error_t = spline_generator_test_t::error_t;
        using result_t = spline_generator_test_t::assembly_result_t;

        mock_assembler_t* mock = nullptr;
        auto operator()(unassembled_state_t state, spline_t& spline) -> result_t { return mock->call(state, spline); }
    };

    using generator_t = spline_generator_t<scalar_t, x_t, spline_t, typestates_t, refinement_pool_t,
        refinement_seeder_t, refiner_t, assembler_t>;

    generator_t generator{
        refinement_seeder_t{&mock_seeder},
        refiner_t{&mock_refiner},
        assembler_t{&mock_assembler},
        workspace_t{&workspace_empty},
    };

    spline_t spline;

    static auto successful_seed(initial_state_t state) -> seeding_result_t
    {
        return unrefined_state_t{.workspace = state.ws, .id = 100};
    }

    static auto successful_refinement(unrefined_state_t state) -> refinement_result_t
    {
        return unassembled_state_t{.workspace = state.workspace, .id = 200};
    }
};

TEST_F(spline_generator_test_t, forwards_states_and_target)
{
    InSequence seq;
    void const* expected_target_address = nullptr;
    workspace_t* expected_workspace_address = nullptr;

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](initial_state_t state, auto const& passed_target, auto) {
        expected_target_address = &passed_target;
        expected_workspace_address = &state.ws;
        return successful_seed(state);
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce([&](unrefined_state_t state, auto const& passed_target) {
        EXPECT_EQ(static_cast<void const*>(&passed_target), expected_target_address);
        EXPECT_EQ(&state.workspace, expected_workspace_address);
        EXPECT_EQ(state.id, 100);
        return successful_refinement(state);
    });
    EXPECT_CALL(mock_assembler, call(_, _)).WillOnce([&](unassembled_state_t state, spline_t&) -> assembly_result_t {
        EXPECT_EQ(&state.workspace, expected_workspace_address);
        EXPECT_EQ(state.id, 200);
        return {};
    });

    EXPECT_TRUE(generator(spline, target, {}));
}

TEST_F(spline_generator_test_t, preserves_exact_critical_points_while_sorting_and_deduplicating)
{
    auto const critical_points = critical_points_t{x_t{11}, x_t{5}, x_t{7}, x_t{5}};
    auto const expected = critical_points_t{x_t{5}, x_t{7}, x_t{11}};

    EXPECT_CALL(mock_seeder, call(_, _, expected)).WillOnce([](initial_state_t state, auto const&, auto) {
        return successful_seed(state);
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce([](unrefined_state_t state, auto const&) {
        return successful_refinement(state);
    });
    EXPECT_CALL(mock_assembler, call(_, _)).WillOnce(Return(assembly_result_t{}));

    EXPECT_TRUE(generator(spline, target, critical_points));
}

TEST_F(spline_generator_test_t, passes_workspace_reference_to_initial_state_and_assembler)
{
    workspace_t* workspace_address = nullptr;
    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](initial_state_t state, auto const&, auto) {
        EXPECT_TRUE(state.ws.empty());
        workspace_address = &state.ws;
        return successful_seed(state);
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce([](unrefined_state_t state, auto const&) {
        return successful_refinement(state);
    });
    EXPECT_CALL(mock_assembler, call(_, _)).WillOnce([&](unassembled_state_t state, spline_t&) -> assembly_result_t {
        EXPECT_EQ(&state.workspace, workspace_address);
        return {};
    });

    EXPECT_TRUE(generator(spline, target, {}));
}

TEST_F(spline_generator_test_t, passes_spline_reference_to_assembler)
{
    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([](initial_state_t state, auto const&, auto) {
        return successful_seed(state);
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce([](unrefined_state_t state, auto const&) {
        return successful_refinement(state);
    });
    EXPECT_CALL(mock_assembler, call(_, Ref(spline))).WillOnce(Return(assembly_result_t{}));

    EXPECT_TRUE(generator(spline, target, {}));
}

TEST_F(spline_generator_test_t, seeding_failure_clears_workspace_without_assembling_or_mutating_spline)
{
    auto const failure = error_t{
        .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
        .left = x_t{3},
        .right = x_t{4},
    };
    auto const original_spline = spline;

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](auto, auto const&, auto) -> seeding_result_t {
        workspace_empty = false;
        return std::unexpected{failure};
    });

    auto const result = generator(spline, target, {});

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), failure);
    EXPECT_EQ(spline.id, original_spline.id);
    EXPECT_TRUE(workspace_empty);
}

TEST_F(spline_generator_test_t, failure_is_returned_without_assembling_or_mutating_spline)
{
    auto const failure = error_t{
        .reason = spline_construction_error_reason_t::segment_budget_exhausted,
        .left = x_t{1},
        .right = x_t{2},
    };
    auto const original_spline = spline;

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([](initial_state_t state, auto const&, auto) {
        return successful_seed(state);
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(std::unexpected{failure}));

    auto const result = generator(spline, target, {});

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), failure);
    EXPECT_EQ(spline.id, original_spline.id);
    EXPECT_TRUE(workspace_empty);
}

TEST_F(spline_generator_test_t, assembly_failure_is_preserved_and_clears_workspace)
{
    auto const failure = error_t{
        .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
        .left = x_t{5},
        .right = x_t{6},
    };

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([](initial_state_t state, auto const&, auto) {
        return successful_seed(state);
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce([](unrefined_state_t state, auto const&) {
        return successful_refinement(state);
    });
    EXPECT_CALL(mock_assembler, call(_, Ref(spline))).WillOnce([&](auto, auto&) -> assembly_result_t {
        workspace_empty = false;
        return std::unexpected{failure};
    });

    auto const result = generator(spline, target, {});

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), failure);
    EXPECT_TRUE(workspace_empty);
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG
TEST_F(spline_generator_test_t, asserts_when_initial_workspace_dirty)
{
    workspace_empty = false;
    EXPECT_DEATH(static_cast<void>(generator(spline, target, {})), "workspace_.empty");
}
#endif

struct spline_generator_factory_test_t : Test
{
    struct subdivision_predicate_t
    {
        float_t global_tolerance;
    };

    struct tangent_extender_t
    {
        float_t y_limit;
        int_t extract_float;
    };

    struct refiner_t
    {
        subdivision_predicate_t requires_subdivision;
        int_t subdivide;
    };

    struct assembler_t
    {
        int_t sort_intervals;
        int_t unzip_intervals;
        int_t pad_keys;
        tangent_extender_t extend_tangent;
    };

    struct spline_generator_t
    {
        int_t seeder;
        refiner_t refiner;
        assembler_t assembler;
    };

    struct policy_t
    {
        using scalar_t = float_t;
        using refinement_pool_seeder_t = int_t;
        using subdivision_predicate_t = spline_generator_factory_test_t::subdivision_predicate_t;
        using refiner_t = spline_generator_factory_test_t::refiner_t;
        using tangent_extender_t = spline_generator_factory_test_t::tangent_extender_t;
        using assembler_t = spline_generator_factory_test_t::assembler_t;
        using spline_generator_t = spline_generator_factory_test_t::spline_generator_t;

        static constexpr auto y_limit = 1000.0;
    };
};

TEST_F(spline_generator_factory_test_t, wires_runtime_and_policy_constants)
{
    auto const generator = spline_generator_factory_t<policy_t>{}(1.5);

    EXPECT_EQ(generator.refiner.requires_subdivision.global_tolerance, 1.5);
    EXPECT_EQ(generator.assembler.extend_tangent.y_limit, policy_t::y_limit);
}

} // namespace
} // namespace crv::spline
