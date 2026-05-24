// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "refinement_pool_seeder.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <vector>

namespace crv::spline {
namespace {

struct spline_refinement_pool_seeder_test_t : Test
{
    using x_t = fixed_t<int_t, 8>;
    using scalar_t = float_t;
    using jet_t = jet_t<scalar_t>;
    using function_sample_t = int_t;

    struct refinement_pool_t
    {
        bool empty_;

        auto empty() const noexcept -> bool { return empty_; }
    };

    struct workspace_t
    {
        refinement_pool_t refinement_pool;
    };
    workspace_t workspace;

    struct common_typestate_t
    {
        workspace_t& workspace;
    };

    struct next_typestate_t : common_typestate_t
    {};

    struct typestate_t : common_typestate_t
    {
        using next_t = next_typestate_t;
    };

    struct mock_sample_target_function_t
    {
        virtual ~mock_sample_target_function_t() = default;

        MOCK_METHOD(function_sample_t, call, (jet_t const& jet), (const, noexcept));
    };
    StrictMock<mock_sample_target_function_t> mock_sample_target_function;

    struct sample_target_function_t
    {
        mock_sample_target_function_t* mock = nullptr;
        auto operator()(jet_t const& jet) const noexcept -> function_sample_t { return mock->call(jet); }
    };
    sample_target_function_t sample_target_function{&mock_sample_target_function};

    struct mock_span_decomposer_t
    {
        virtual ~mock_span_decomposer_t() = default;

        MOCK_METHOD(function_sample_t, call,
            (sample_target_function_t const& sample_target_function, function_sample_t const& left_function_sample,
                x_t const& left_critical_point, x_t const& right_critical_point, refinement_pool_t& refinement_pool),
            (const, noexcept));
    };
    StrictMock<mock_span_decomposer_t> mock_decompose_span;

    struct span_decomposer_t
    {
        using x_t = x_t;
        using scalar_t = scalar_t;
        using jet_t = jet_t;
        using function_sample_t = function_sample_t;

        mock_span_decomposer_t* mock = nullptr;

        auto operator()(auto const& sample_target_function, function_sample_t const& left_function_sample,
            x_t const& left_critical_point, x_t const& right_critical_point,
            refinement_pool_t& refinement_pool) const noexcept -> function_sample_t
        {
            return mock->call(sample_target_function, left_function_sample, left_critical_point, right_critical_point,
                refinement_pool);
        }
    };

    static constexpr auto log2_domain_end = 4;
    using sut_t = refinement_pool_seeder_t<typestate_t, span_decomposer_t, log2_domain_end>;
    using critical_points_t = sut_t::critical_points_t;

    sut_t sut{.decompose_span = span_decomposer_t{&mock_decompose_span}};

    auto expect_init(function_sample_t result) noexcept -> void
    {
        EXPECT_CALL(mock_sample_target_function, call(jet_t{0.0, 1.0})).WillOnce(Return(result));
    }

    auto expect_iteration(function_sample_t left_function_sample, x_t const& left_critical_point,
        x_t const& right_critical_point, function_sample_t result)
    {
        EXPECT_CALL(mock_decompose_span,
            call(Ref(sample_target_function), left_function_sample, left_critical_point, right_critical_point,
                Ref(workspace.refinement_pool)))
            .WillOnce(Return(result));
    }

    auto expect_end(function_sample_t left_function_sample, x_t const& left_critical_point) noexcept -> void
    {
        EXPECT_CALL(mock_decompose_span,
            call(Ref(sample_target_function), left_function_sample, left_critical_point, sut_t::domain_end,
                Ref(workspace.refinement_pool)))
            .WillOnce(Return(-1));
    }
};

TEST_F(spline_refinement_pool_seeder_test_t, no_critical_points)
{
    auto const critical_points = critical_points_t{};

    expect_init(1);
    expect_end(1, x_t{0});

    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function, critical_points);

    EXPECT_EQ(&workspace, &actual.workspace);
}

TEST_F(spline_refinement_pool_seeder_test_t, one_critical_point)
{
    auto const critical_points = critical_points_t{x_t::literal(101)};

    expect_init(1);
    expect_iteration(1, x_t{0}, critical_points[0], 2);
    expect_end(2, critical_points[0]);

    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function, critical_points);

    EXPECT_EQ(&workspace, &actual.workspace);
}

TEST_F(spline_refinement_pool_seeder_test_t, two_critical_points)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(102)};

    expect_init(1);
    expect_iteration(1, x_t{0}, critical_points[0], 2);
    expect_iteration(2, critical_points[0], critical_points[1], 3);
    expect_end(3, critical_points[1]);

    next_typestate_t const actual = sut(typestate_t{workspace}, sample_target_function, critical_points);

    EXPECT_EQ(&workspace, &actual.workspace);
}

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_unique)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(101)};

    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function, critical_points), "unique");
}

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_monotonically_increasing)
{
    auto const critical_points = critical_points_t{x_t::literal(101), x_t::literal(100)};

    EXPECT_DEBUG_DEATH(
        sut(typestate_t{workspace}, sample_target_function, critical_points), "monotonically increasing");
}

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_greater_than_zero)
{
    auto const critical_points = critical_points_t{x_t{0}};

    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function, critical_points), "in \\(0, domain_end\\)");
}

TEST_F(spline_refinement_pool_seeder_test_t, critical_points_must_be_less_than_domain_end)
{
    auto const critical_points = critical_points_t{sut_t::domain_end};

    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function, critical_points), "in \\(0, domain_end\\)");
}

TEST_F(spline_refinement_pool_seeder_test_t, refinement_pool_must_be_empty)
{
    auto const critical_points = critical_points_t{};
    workspace.refinement_pool.empty_ = false;

    EXPECT_DEBUG_DEATH(sut(typestate_t{workspace}, sample_target_function, critical_points), "empty");
}

#endif

} // namespace
} // namespace crv::spline
