// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline_generator.hpp"
#include <crv/test/test.hpp>
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

    struct x_t
    {
        int_t value;
        constexpr auto operator<=>(x_t const&) const noexcept = default;
    };

    using critical_points_t = std::vector<x_t>;

    struct spline_t
    {};

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
        int_t id = 0;
        constexpr auto operator==(unrefined_state_t const&) const noexcept -> bool = default;
    };

    struct unassembled_state_t
    {
        int_t id = 0;
        constexpr auto operator==(unassembled_state_t const&) const noexcept -> bool = default;
    };

    struct typestates_t
    {
        using workspace_t = workspace_t;
        using initial_t = initial_state_t;
    };

    struct mock_refinement_seeder_t
    {
        virtual ~mock_refinement_seeder_t() = default;
        MOCK_METHOD(
            unrefined_state_t, call, (initial_state_t, transfer_sampler_t<target_t> const&, critical_points_t));
    };
    StrictMock<mock_refinement_seeder_t> mock_seeder;

    struct refinement_seeder_t
    {
        using critical_points_t = spline_generator_test_t::critical_points_t;
        mock_refinement_seeder_t* mock;

        auto operator()(initial_state_t state, auto const& sampler, critical_points_t const& critical_points)
            -> unrefined_state_t
        {
            return mock->call(state, sampler, critical_points);
        }
    };

    struct mock_refiner_t
    {
        virtual ~mock_refiner_t() = default;
        MOCK_METHOD(unassembled_state_t, call, (unrefined_state_t, transfer_sampler_t<target_t> const&));
    };
    StrictMock<mock_refiner_t> mock_refiner;

    struct refiner_t
    {
        mock_refiner_t* mock;
        auto operator()(unrefined_state_t state, auto const& sampler) { return mock->call(state, sampler); }
    };

    struct mock_assembler_t
    {
        virtual ~mock_assembler_t() = default;
        MOCK_METHOD(void, call, (unassembled_state_t, spline_t&));
    };
    StrictMock<mock_assembler_t> mock_assembler;

    struct assembler_t
    {
        mock_assembler_t* mock = nullptr;
        auto operator()(unassembled_state_t state, spline_t& spline) { mock->call(state, spline); }
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
    unrefined_state_t const unrefined_state{100};
    unassembled_state_t const unassembled_state{200};
};

TEST_F(spline_generator_test_t, forwards_states_and_sampler)
{
    InSequence seq;
    void const* expected_sampler_address = nullptr;

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](initial_state_t, auto const& sampler, auto) {
        expected_sampler_address = &sampler;
        return unrefined_state;
    });
    EXPECT_CALL(mock_refiner, call(unrefined_state, _)).WillOnce([&](unrefined_state_t, auto const& sampler) {
        EXPECT_EQ(static_cast<void const*>(&sampler), expected_sampler_address);
        return unassembled_state;
    });
    EXPECT_CALL(mock_assembler, call(unassembled_state, _));

    generator(spline, target, {});
}

TEST_F(spline_generator_test_t, preserves_exact_critical_points_while_sorting_and_deduplicating)
{
    auto const critical_points = critical_points_t{x_t{11}, x_t{5}, x_t{7}, x_t{5}};
    auto const expected = critical_points_t{x_t{5}, x_t{7}, x_t{11}};

    EXPECT_CALL(mock_seeder, call(_, _, expected)).WillOnce(Return(unrefined_state));
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(unassembled_state));
    EXPECT_CALL(mock_assembler, call(_, _));

    generator(spline, target, critical_points);
}

TEST_F(spline_generator_test_t, passes_workspace_reference_to_initial_state)
{
    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](initial_state_t state, auto const&, auto) {
        EXPECT_TRUE(state.ws.empty());
        return unrefined_state;
    });
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(unassembled_state));
    EXPECT_CALL(mock_assembler, call(_, _));

    generator(spline, target, {});
}

TEST_F(spline_generator_test_t, passes_spline_reference_to_assembler)
{
    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce(Return(unrefined_state));
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(unassembled_state));
    EXPECT_CALL(mock_assembler, call(_, Ref(spline)));

    generator(spline, target, {});
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG
TEST_F(spline_generator_test_t, asserts_when_initial_workspace_dirty)
{
    workspace_empty = false;
    EXPECT_DEATH(generator(spline, target, {}), "workspace_.empty");
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
