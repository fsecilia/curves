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
    static constexpr auto target_function = [](float x) { return x; };
    using target_function_t = decltype(target_function);
    function_sampler_t<target_function_t> const sample_target_function{target_function};

    using scalar_t = float_t;

    struct x_t
    {
        int_t value;
        constexpr auto operator==(x_t const&) const noexcept -> bool = default;
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

    struct mock_critical_point_conditioner_t
    {
        virtual ~mock_critical_point_conditioner_t() = default;
        MOCK_METHOD(critical_points_t, call, (critical_points_t));
    };
    StrictMock<mock_critical_point_conditioner_t> mock_critical_point_conditioner;

    struct critical_point_conditioner_t
    {
        using critical_points_t = critical_points_t;

        mock_critical_point_conditioner_t* mock;

        auto operator()(critical_points_t critical_points) const -> critical_points_t
        {
            return mock->call(critical_points);
        }
    };

    struct mock_refinement_seeder_t
    {
        virtual ~mock_refinement_seeder_t() = default;
        MOCK_METHOD(unrefined_state_t, call,
            (initial_state_t, function_sampler_t<target_function_t const> const&, std::vector<x_t>));
    };
    StrictMock<mock_refinement_seeder_t> mock_seeder;

    struct refinement_seeder_t
    {
        using critical_points_t = critical_points_t;

        mock_refinement_seeder_t* mock;

        auto operator()(initial_state_t state, function_sampler_t<target_function_t const> const& sampler,
            critical_points_t const& critical_points) -> unrefined_state_t
        {
            return mock->call(state, sampler, critical_points);
        }
    };

    struct mock_refiner_t
    {
        virtual ~mock_refiner_t() = default;
        MOCK_METHOD(unassembled_state_t, call, (unrefined_state_t, function_sampler_t<target_function_t const> const&));
    };
    StrictMock<mock_refiner_t> mock_refiner;

    struct refiner_t
    {
        mock_refiner_t* mock;
        auto operator()(unrefined_state_t state, function_sampler_t<target_function_t const> const& sampler)
        {
            return mock->call(state, sampler);
        }
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

    using generator_t = spline_generator_t<scalar_t, x_t, spline_t, typestates_t, critical_point_conditioner_t,
        refinement_pool_t, refinement_seeder_t, refiner_t, assembler_t>;

    generator_t generator{
        critical_point_conditioner_t{&mock_critical_point_conditioner},
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

    EXPECT_CALL(mock_critical_point_conditioner, call(critical_points_t{})).WillOnce(Return(critical_points_t{}));

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](initial_state_t, auto const& sampler_address, auto) {
        expected_sampler_address = &sampler_address;
        return unrefined_state;
    });

    EXPECT_CALL(mock_refiner, call(unrefined_state, _)).WillOnce([&](unrefined_state_t, auto const& sampler_address) {
        EXPECT_EQ(static_cast<void const*>(&sampler_address), expected_sampler_address);
        return unassembled_state;
    });

    EXPECT_CALL(mock_assembler, call(unassembled_state, _));

    generator(spline, sample_target_function, {});
}

TEST_F(spline_generator_test_t, forwards_critical_points)
{
    auto const initial_critical_points = critical_points_t{x_t{5}, x_t{7}, x_t{11}};
    auto const conditioned_critical_points = critical_points_t{x_t{2}, x_t{3}, x_t{5}, x_t{7}};

    EXPECT_CALL(mock_critical_point_conditioner, call(initial_critical_points))
        .WillOnce(Return(conditioned_critical_points));
    EXPECT_CALL(mock_seeder, call(_, _, conditioned_critical_points)).WillOnce(Return(unrefined_state));

    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(unassembled_state));
    EXPECT_CALL(mock_assembler, call(_, _));

    generator(spline, sample_target_function, initial_critical_points);
}

TEST_F(spline_generator_test_t, passes_workspace_reference_to_initial_state)
{
    EXPECT_CALL(mock_critical_point_conditioner, call(critical_points_t{})).WillOnce(Return(critical_points_t{}));

    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce([&](initial_state_t state, auto const&, auto) {
        EXPECT_TRUE(state.ws.empty());
        return unrefined_state;
    });

    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(unassembled_state));
    EXPECT_CALL(mock_assembler, call(_, _));

    generator(spline, sample_target_function, {});
}

TEST_F(spline_generator_test_t, passes_spline_reference_to_assembler)
{
    EXPECT_CALL(mock_critical_point_conditioner, call(critical_points_t{})).WillOnce(Return(critical_points_t{}));
    EXPECT_CALL(mock_seeder, call(_, _, _)).WillOnce(Return(unrefined_state));
    EXPECT_CALL(mock_refiner, call(_, _)).WillOnce(Return(unassembled_state));

    EXPECT_CALL(mock_assembler, call(_, Ref(spline)));

    generator(spline, sample_target_function, {});
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_generator_test_t, asserts_when_initial_workspace_dirty)
{
    workspace_empty = false;
    EXPECT_DEATH(generator(spline, sample_target_function, {}), "workspace_.empty");
}

#endif

} // namespace
} // namespace crv::spline
