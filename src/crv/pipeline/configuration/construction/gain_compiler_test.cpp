// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "gain_compiler.hpp"
#include <crv/model/curves/log_normal.hpp>
#include <crv/model/curves/synchronous.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <gmock/gmock.h>

namespace crv::pipeline::configuration::construction {
namespace {

using spline_policy_t = spline::default_spline_policy_t<float_t, spline::prod_pipeline_config_t>;
using scalar_t = spline_policy_t::scalar_t;
using x_t = spline_policy_t::x_t;
using critical_points_t = critical_point_builder_t<scalar_t, x_t>::result_t;
using spline_result_t = spline::spline_generation_result_t<x_t>;
enum class shaping_error_t
{
    failed,
};

struct critical_point_builder_test_t : Test
{
    static constexpr auto sync_speed = scalar_t{5.123456789012345};
    model::curves::synchronous_t::evaluator_t<scalar_t> curve{
        model::curves::synchronous_t::params_t<scalar_t>{2.0, 3.0, 0.5, sync_speed}};
    critical_point_builder_t<scalar_t, x_t> sut;
};

TEST_F(critical_point_builder_test_t, preserves_authored_point_for_integration)
{
    auto const points = sut(curve, scalar_t{spline_policy_t::domain_end});
    EXPECT_EQ(points.integration, std::vector<scalar_t>{sync_speed});
}

TEST_F(critical_point_builder_test_t, quantizes_independently_for_spline_geometry)
{
    auto const points = sut(curve, scalar_t{spline_policy_t::domain_end});
    EXPECT_EQ(points.spline, std::vector<x_t>{to_fixed<x_t>(sync_speed)});
}

TEST_F(critical_point_builder_test_t, omits_points_outside_runtime_domain)
{
    auto curve = model::curves::synchronous_t::evaluator_t<scalar_t>{
        model::curves::synchronous_t::params_t<scalar_t>{2.0, 3.0, 0.5, 300.0}};
    EXPECT_TRUE(sut(curve, scalar_t{spline_policy_t::domain_end}).integration.empty());
}

TEST_F(critical_point_builder_test_t, log_normal_has_no_mandatory_points)
{
    auto const curve = model::curves::log_normal_t::evaluator_t<scalar_t>{
        model::curves::log_normal_t::params_t<scalar_t>{2.0 / 3.0, 1.5, 1.0, 0.5}};
    EXPECT_TRUE(sut(curve, scalar_t{spline_policy_t::domain_end}).integration.empty());
}

struct gain_compiler_test_t : Test
{
    struct shaped_curve_t
    {
        template <typename value_t> constexpr auto operator()(value_t value) const noexcept -> value_t { return value; }
    };

    using shaping_result_t = std::expected<shaped_curve_t, shaping_error_t>;

    struct target_t
    {};

    struct target_result_t
    {
        target_t target;
        scalar_t achieved_error{};
        scalar_t max_error{};
        bool refinement_limited{};
    };

    struct mock_t
    {
        MOCK_METHOD(shaping_result_t, shape_curve, (model::common_curve_config_t const*, scalar_t), (const));
        MOCK_METHOD(critical_points_t, critical_points, (scalar_t), (const));
        MOCK_METHOD(target_result_t, sensitivity_target, (scalar_t, std::vector<scalar_t> const&), (const));
        MOCK_METHOD(spline_result_t, spline, (pipeline_t::gain_t*, scalar_t, std::vector<x_t> const&), (const));
    };
    StrictMock<mock_t> mock;

    struct shaped_curve_builder_delegate_t
    {
        using error_t = shaping_error_t;

        mock_t* mock;

        template <typename curve_t>
        auto operator()(curve_t, model::common_curve_config_t const& common, scalar_t domain_end) const
            -> shaping_result_t
        {
            return mock->shape_curve(&common, domain_end);
        }
    };

    struct critical_point_builder_delegate_t
    {
        mock_t* mock;

        auto operator()(shaped_curve_t const&, scalar_t domain_end) const -> critical_points_t
        {
            return mock->critical_points(domain_end);
        }
    };

    struct sensitivity_target_builder_delegate_t
    {
        mock_t* mock;

        template <typename curve_t>
        auto operator()(curve_t, scalar_t domain_end, std::vector<scalar_t> const& critical_points) const
            -> target_result_t
        {
            return mock->sensitivity_target(domain_end, critical_points);
        }
    };

    struct spline_factory_delegate_t
    {
        mock_t* mock;

        template <typename target_t>
        auto operator()(pipeline_t::gain_t& gain, target_t&&, scalar_t tolerance,
            std::vector<x_t> critical_points) const -> spline_result_t
        {
            return mock->spline(&gain, tolerance, critical_points);
        }
    };

    using sut_t = gain_compiler_t<spline_policy_t, shaped_curve_builder_delegate_t, critical_point_builder_delegate_t,
        sensitivity_target_builder_delegate_t, spline_factory_delegate_t>;

    model::curves_t curves;
    pipeline_t::gain_t gain{};
    sut_t sut{{&mock}, {&mock}, {&mock}, {&mock}};
};

TEST_F(gain_compiler_test_t, forwards_curve_through_sensitivity_construction)
{
    auto const integration_points = std::vector<scalar_t>{1.25};
    auto const spline_points = std::vector<x_t>{to_fixed<x_t>(1.25)};
    EXPECT_CALL(mock, critical_points(scalar_t{spline_policy_t::domain_end}))
        .WillOnce(Return(critical_points_t{integration_points, spline_points}));
    EXPECT_CALL(mock, shape_curve(_, scalar_t{spline_policy_t::domain_end})).WillOnce(Return(shaping_result_t{}));
    EXPECT_CALL(mock, sensitivity_target(scalar_t{spline_policy_t::domain_end}, integration_points))
        .WillOnce(Return(target_result_t{}));
    EXPECT_CALL(mock, spline(&gain, scalar_t{spline_policy_t::spline_gain_tolerance}, spline_points))
        .WillOnce(Return(spline_result_t{}));

    EXPECT_TRUE(sut(gain, curves));
}

TEST_F(gain_compiler_test_t, shaping_failure_is_preserved)
{
    auto const failure = shaping_error_t::failed;
    EXPECT_CALL(mock, shape_curve(_, scalar_t{spline_policy_t::domain_end})).WillOnce(Return(std::unexpected{failure}));

    auto const result = sut(gain, curves);

    EXPECT_EQ(std::get<shaping_error_t>(result.error().detail), failure);
}

TEST_F(gain_compiler_test_t, refinement_limit_is_preserved)
{
    auto const target_result = target_result_t{
        .target = {}, .achieved_error = scalar_t{3}, .max_error = scalar_t{2}, .refinement_limited = true};
    EXPECT_CALL(mock, critical_points).WillOnce(Return(critical_points_t{}));
    EXPECT_CALL(mock, shape_curve(_, scalar_t{spline_policy_t::domain_end})).WillOnce(Return(shaping_result_t{}));
    EXPECT_CALL(mock, sensitivity_target).WillOnce(Return(target_result));

    auto const result = sut(gain, curves);

    EXPECT_EQ(std::get<sensitivity_refinement_error_t<scalar_t>>(result.error().detail),
        (sensitivity_refinement_error_t<scalar_t>{.achieved_error = 3, .max_error = 2}));
}

TEST_F(gain_compiler_test_t, spline_failure_is_preserved)
{
    auto const failure = spline::spline_generation_error_t<x_t>{
        .reason = spline::spline_generation_error_reason_t::segment_budget_exhausted,
        .left = x_t{1},
        .right = x_t{2},
    };
    EXPECT_CALL(mock, critical_points).WillOnce(Return(critical_points_t{}));
    EXPECT_CALL(mock, shape_curve(_, scalar_t{spline_policy_t::domain_end})).WillOnce(Return(shaping_result_t{}));
    EXPECT_CALL(mock, sensitivity_target).WillOnce(Return(target_result_t{}));
    EXPECT_CALL(mock, spline).WillOnce(Return(spline_result_t{.error = failure}));

    auto const result = sut(gain, curves);

    EXPECT_EQ(std::get<spline::spline_generation_error_t<x_t>>(result.error().detail), failure);
}

} // namespace
} // namespace crv::pipeline::configuration::construction
