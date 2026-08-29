// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "compact_output_limiter.hpp"
#include <crv/model/shaping/output_limited_curve.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <gmock/gmock.h>
#include <limits>
#include <string_view>

namespace crv::shaping::transforms {
namespace {

struct shaping_transforms_compact_output_limiter_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;

    struct mock_transition_t
    {
        virtual ~mock_transition_t() = default;

        MOCK_METHOD(scalar_t, value, (scalar_t), (const, noexcept));
        MOCK_METHOD(scalar_t, derivative, (scalar_t), (const, noexcept));
        MOCK_METHOD(scalar_t, antiderivative, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_transition_t> mock_transition;

    struct transition_t
    {
        mock_transition_t* mock;

        auto operator()(scalar_t u) const noexcept -> scalar_t { return mock->value(u); }
        auto derivative(scalar_t u) const noexcept -> scalar_t { return mock->derivative(u); }
        auto antiderivative(scalar_t u) const noexcept -> scalar_t { return mock->antiderivative(u); }

        auto operator()(jet_t u) const noexcept -> jet_t
        {
            auto const value = primal(u);
            return {operator()(value), derivative(value) * tangent(u)};
        }

        auto antiderivative(jet_t u) const noexcept -> jet_t
        {
            auto const value = primal(u);
            return {antiderivative(value), operator()(value) * tangent(u)};
        }
    };

    struct mock_curve_t
    {
        virtual ~mock_curve_t() = default;

        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
    };
    StrictMock<mock_curve_t> mock_curve;

    struct curve_t
    {
        mock_curve_t* mock;

        auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
    };

    using upper_t = upper_output_limiter_t<scalar_t, transition_t>;
    using lower_t = lower_output_limiter_t<scalar_t, transition_t>;

    static constexpr auto bound = scalar_t{2};
    static constexpr auto delta_y = scalar_t{0.5};
    static constexpr auto half_integral = scalar_t{0.25};
    static constexpr auto tolerance = scalar_t{1e-12};

    auto make_upper(scalar_t requested_bound = bound, scalar_t requested_delta_y = delta_y,
        scalar_t j_half = half_integral) -> upper_t
    {
        EXPECT_CALL(mock_transition, antiderivative(scalar_t{0.5})).WillOnce(Return(j_half));
        return upper_t::make(requested_bound, requested_delta_y, transition_t{&mock_transition}).value();
    }

    auto make_lower(scalar_t requested_bound = bound, scalar_t requested_delta_y = delta_y,
        scalar_t j_half = half_integral) -> lower_t
    {
        EXPECT_CALL(mock_transition, antiderivative(scalar_t{0.5})).WillOnce(Return(j_half));
        return lower_t::make(requested_bound, requested_delta_y, transition_t{&mock_transition}).value();
    }
};

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_derives_log_half_width_from_linear_delta_y)
{
    auto const sut = make_upper();
    auto const expected = -std::log1p(-delta_y / bound) / (scalar_t{2} * half_integral);
    EXPECT_NEAR(sut.support()->half_width, expected, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_derives_log_half_width_from_linear_delta_y)
{
    auto const sut = make_lower();
    auto const expected = std::log1p(delta_y / bound) / (scalar_t{2} * half_integral);
    EXPECT_NEAR(sut.support()->half_width, expected, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_nominal_bound_deflects_down_by_delta_y)
{
    EXPECT_CALL(mock_transition, antiderivative(_)).Times(2).WillRepeatedly(Return(half_integral));
    auto const sut = upper_t::make(bound, delta_y, transition_t{&mock_transition}).value();
    EXPECT_NEAR(sut(bound), bound - delta_y, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_nominal_bound_deflects_up_by_delta_y)
{
    EXPECT_CALL(mock_transition, antiderivative(_)).Times(2).WillRepeatedly(Return(half_integral));
    auto const sut = lower_t::make(bound, delta_y, transition_t{&mock_transition}).value();
    EXPECT_NEAR(sut(bound), bound + delta_y, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_is_exact_identity_below_support)
{
    auto const sut = make_upper();
    auto const output = std::exp(sut.support()->lower_log - scalar_t{1});
    EXPECT_EQ(sut(output), output);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_is_exact_plateau_above_support)
{
    auto const sut = make_upper();
    auto const output = std::exp(sut.support()->upper_log + scalar_t{1});
    EXPECT_EQ(sut(output), bound);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_is_exact_plateau_below_support)
{
    auto const sut = make_lower();
    auto const output = std::exp(sut.support()->lower_log - scalar_t{1});
    EXPECT_EQ(sut(output), bound);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_is_exact_identity_above_support)
{
    auto const sut = make_lower();
    auto const output = std::exp(sut.support()->upper_log + scalar_t{1});
    EXPECT_EQ(sut(output), output);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_support_lower_boundary_is_identity_by_geometry)
{
    auto const sut = make_upper();
    EXPECT_EQ(sut.classify_log(sut.support()->lower_log), output_limiter_region_t::identity);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_support_upper_boundary_is_plateau_by_geometry)
{
    auto const sut = make_upper();
    EXPECT_EQ(sut.classify_log(sut.support()->upper_log), output_limiter_region_t::plateau);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_support_lower_boundary_is_plateau_by_geometry)
{
    auto const sut = make_lower();
    EXPECT_EQ(sut.classify_log(sut.support()->lower_log), output_limiter_region_t::plateau);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_support_upper_boundary_is_identity_by_geometry)
{
    auto const sut = make_lower();
    EXPECT_EQ(sut.classify_log(sut.support()->upper_log), output_limiter_region_t::identity);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, upper_transition_uses_antiderivative_formula)
{
    auto const u = scalar_t{0.25};
    auto const j = scalar_t{0.04};
    auto const sut = make_upper();
    auto const support = *sut.support();
    auto const output = std::exp(support.upper_log - scalar_t{2} * support.half_width * u);
    EXPECT_CALL(mock_transition, antiderivative(DoubleNear(u, tolerance))).WillOnce(Return(j));
    auto const expected = bound * std::exp(-scalar_t{2} * support.half_width * j);
    EXPECT_NEAR(sut(output), expected, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, lower_transition_uses_antiderivative_formula)
{
    auto const u = scalar_t{0.25};
    auto const j = scalar_t{0.04};
    auto const sut = make_lower();
    auto const support = *sut.support();
    auto const output = std::exp(support.lower_log + scalar_t{2} * support.half_width * u);
    EXPECT_CALL(mock_transition, antiderivative(DoubleNear(u, tolerance))).WillOnce(Return(j));
    auto const expected = bound * std::exp(scalar_t{2} * support.half_width * j);
    EXPECT_NEAR(sut(output), expected, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, transition_jet_uses_analytic_derivative_multiplier)
{
    auto const u = scalar_t{0.25};
    auto const j = scalar_t{0.04};
    auto const h = scalar_t{0.3};
    auto const input_tangent = scalar_t{1.7};
    auto const sut = make_upper();
    auto const support = *sut.support();
    auto const output = std::exp(support.upper_log - scalar_t{2} * support.half_width * u);
    EXPECT_CALL(mock_transition, antiderivative(DoubleNear(u, tolerance))).WillOnce(Return(j));
    EXPECT_CALL(mock_transition, value(DoubleNear(u, tolerance))).WillOnce(Return(h));
    auto const limited = bound * std::exp(-scalar_t{2} * support.half_width * j);
    auto const expected = input_tangent * limited / output * h;
    EXPECT_NEAR(sut(jet_t{output, input_tangent}).df, expected, tolerance);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, plateau_jet_has_exact_zero_tangent)
{
    auto const sut = make_lower();
    auto const output = std::exp(sut.support()->lower_log - scalar_t{1});
    EXPECT_EQ(sut(jet_t{output, scalar_t{9}}), (jet_t{bound, scalar_t{0}}));
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, identity_jet_is_exactly_preserved)
{
    auto const sut = make_upper();
    auto const output = std::exp(sut.support()->lower_log - scalar_t{1});
    auto const input = jet_t{output, scalar_t{9}};
    EXPECT_EQ(sut(input), input);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, positive_lower_plateau_skips_curve_jet)
{
    auto const limiter = make_lower();
    auto const curve_value = std::exp(limiter.support()->lower_log - scalar_t{1});
    auto const input = jet_t{scalar_t{3}, scalar_t{7}};
    EXPECT_CALL(mock_curve, scalar(primal(input))).WillOnce(Return(curve_value));
    auto const sut = shaping::output_limited_curve_t{limiter, curve_t{&mock_curve}};
    EXPECT_EQ(sut(input), (jet_t{bound, scalar_t{0}}));
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, identity_region_resumes_curve_jet_evaluation)
{
    auto const limiter = make_lower();
    auto const curve_value = std::exp(limiter.support()->upper_log + scalar_t{1});
    auto const input = jet_t{scalar_t{3}, scalar_t{7}};
    auto const curve_jet = jet_t{curve_value, scalar_t{11}};
    EXPECT_CALL(mock_curve, scalar(primal(input))).WillOnce(Return(curve_value));
    EXPECT_CALL(mock_curve, jet(input)).WillOnce(Return(curve_jet));
    auto const sut = shaping::output_limited_curve_t{limiter, curve_t{&mock_curve}};
    EXPECT_EQ(sut(input), curve_jet);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t,
    interior_zero_transition_value_does_not_create_plateau_control_flow)
{
    EXPECT_CALL(mock_transition, antiderivative(_)).Times(2).WillRepeatedly(Return(half_integral));
    auto const limiter = lower_t::make(bound, delta_y, transition_t{&mock_transition}).value();
    auto const input = jet_t{scalar_t{3}, scalar_t{7}};
    auto const curve_jet = jet_t{bound, scalar_t{11}};
    EXPECT_CALL(mock_curve, scalar(primal(input))).WillOnce(Return(bound));
    EXPECT_CALL(mock_curve, jet(input)).WillOnce(Return(curve_jet));
    EXPECT_CALL(mock_transition, value(_)).WillOnce(Return(scalar_t{0}));
    auto const sut = shaping::output_limited_curve_t{limiter, curve_t{&mock_curve}};
    EXPECT_EQ(sut(input).df, scalar_t{0});
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, zero_upper_scalar_skips_curve_completely)
{
    auto const limiter = upper_t::make(scalar_t{0}, scalar_t{0}, transition_t{&mock_transition}).value();
    auto const sut = shaping::output_limited_curve_t{limiter, curve_t{&mock_curve}};
    EXPECT_EQ(sut(scalar_t{3}), scalar_t{0});
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, zero_upper_jet_skips_curve_completely)
{
    auto const limiter = upper_t::make(scalar_t{0}, scalar_t{0}, transition_t{&mock_transition}).value();
    auto const sut = shaping::output_limited_curve_t{limiter, curve_t{&mock_curve}};
    EXPECT_EQ(sut(jet_t{scalar_t{3}, scalar_t{7}}), (jet_t{scalar_t{0}, scalar_t{0}}));
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, zero_lower_jet_is_direct_curve_identity)
{
    auto const limiter = lower_t::make(scalar_t{0}, scalar_t{0}, transition_t{&mock_transition}).value();
    auto const input = jet_t{scalar_t{3}, scalar_t{7}};
    auto const curve_jet = jet_t{scalar_t{5}, scalar_t{11}};
    EXPECT_CALL(mock_curve, jet(input)).WillOnce(Return(curve_jet));
    auto const sut = shaping::output_limited_curve_t{limiter, curve_t{&mock_curve}};
    EXPECT_EQ(sut(input), curve_jet);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, anchor_below_identity_boundary_is_feasible)
{
    auto const sut = make_upper();
    auto const anchor = std::exp(sut.support()->lower_log - scalar_t{1});
    EXPECT_EQ(sut.fixed_anchor_feasibility(anchor), fixed_anchor_feasibility_t::feasible);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, anchor_inside_upper_transition_conflicts)
{
    auto const sut = make_upper();
    EXPECT_EQ(sut.fixed_anchor_feasibility(bound), fixed_anchor_feasibility_t::transition_conflict);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, anchor_above_upper_bound_conflicts)
{
    auto const sut = make_upper();
    EXPECT_EQ(sut.fixed_anchor_feasibility(bound + scalar_t{1}), fixed_anchor_feasibility_t::bound_conflict);
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, zero_upper_only_accepts_zero_fixed_anchor)
{
    auto const sut = upper_t::make(scalar_t{0}, scalar_t{0}, transition_t{&mock_transition}).value();
    EXPECT_EQ(sut.fixed_anchor_feasibility(scalar_t{1}), fixed_anchor_feasibility_t::bound_conflict);
}

struct invalid_limiter_input_t
{
    std::string_view name;
    bool upper;
    float_t bound;
    float_t delta_y;
    output_limiter_error_t error;
};

struct shaping_transforms_compact_output_limiter_invalid_input_test_t
    : shaping_transforms_compact_output_limiter_test_t,
      WithParamInterface<invalid_limiter_input_t>
{};

TEST_P(shaping_transforms_compact_output_limiter_invalid_input_test_t, rejects_invalid_input)
{
    auto const& param = GetParam();
    if (param.upper)
    {
        auto const result = upper_t::make(param.bound, param.delta_y, transition_t{&mock_transition});
        EXPECT_EQ(result, std::unexpected{param.error});
    }
    else
    {
        auto const result = lower_t::make(param.bound, param.delta_y, transition_t{&mock_transition});
        EXPECT_EQ(result, std::unexpected{param.error});
    }
}

INSTANTIATE_TEST_SUITE_P(shaping_transforms_compact_output_limiter_invalid_inputs,
    shaping_transforms_compact_output_limiter_invalid_input_test_t,
    Values(invalid_limiter_input_t{"upper_nan_bound", true, std::numeric_limits<float_t>::quiet_NaN(), 0.1,
               output_limiter_error_t::bound_not_finite},
        invalid_limiter_input_t{"lower_infinite_bound", false, std::numeric_limits<float_t>::infinity(), 0.1,
            output_limiter_error_t::bound_not_finite},
        invalid_limiter_input_t{"upper_negative_bound", true, -1.0, 0.1, output_limiter_error_t::bound_negative},
        invalid_limiter_input_t{"lower_nan_delta", false, 1.0, std::numeric_limits<float_t>::quiet_NaN(),
            output_limiter_error_t::delta_y_not_finite},
        invalid_limiter_input_t{"upper_infinite_delta", true, 1.0, std::numeric_limits<float_t>::infinity(),
            output_limiter_error_t::delta_y_not_finite},
        invalid_limiter_input_t{"lower_negative_delta", false, 1.0, -0.1, output_limiter_error_t::delta_y_negative},
        invalid_limiter_input_t{
            "upper_zero_bound_soft", true, 0.0, 0.1, output_limiter_error_t::zero_bound_requires_zero_delta_y},
        invalid_limiter_input_t{
            "lower_zero_bound_soft", false, 0.0, 0.1, output_limiter_error_t::zero_bound_requires_zero_delta_y},
        invalid_limiter_input_t{"upper_positive_bound_hard", true, 1.0, 0.0,
            output_limiter_error_t::positive_bound_requires_positive_delta_y},
        invalid_limiter_input_t{"lower_positive_bound_hard", false, 1.0, 0.0,
            output_limiter_error_t::positive_bound_requires_positive_delta_y},
        invalid_limiter_input_t{
            "upper_delta_equals_bound", true, 1.0, 1.0, output_limiter_error_t::upper_delta_y_not_below_bound},
        invalid_limiter_input_t{
            "upper_delta_exceeds_bound", true, 1.0, 2.0, output_limiter_error_t::upper_delta_y_not_below_bound}),
    test_name_generator_t<invalid_limiter_input_t>{});

struct invalid_half_integral_t
{
    std::string_view name;
    float_t value;
    output_limiter_error_t error;
};

struct shaping_transforms_compact_output_limiter_invalid_transition_test_t
    : shaping_transforms_compact_output_limiter_test_t,
      WithParamInterface<invalid_half_integral_t>
{};

TEST_P(shaping_transforms_compact_output_limiter_invalid_transition_test_t, rejects_invalid_half_integral)
{
    auto const& param = GetParam();
    EXPECT_CALL(mock_transition, antiderivative(scalar_t{0.5})).WillOnce(Return(param.value));
    auto const result = upper_t::make(bound, delta_y, transition_t{&mock_transition});
    EXPECT_EQ(result, std::unexpected{param.error});
}

INSTANTIATE_TEST_SUITE_P(shaping_transforms_compact_output_limiter_invalid_transition_half_integrals,
    shaping_transforms_compact_output_limiter_invalid_transition_test_t,
    Values(invalid_half_integral_t{"nan", std::numeric_limits<float_t>::quiet_NaN(),
               output_limiter_error_t::transition_half_integral_not_finite},
        invalid_half_integral_t{"infinity", std::numeric_limits<float_t>::infinity(),
            output_limiter_error_t::transition_half_integral_not_finite},
        invalid_half_integral_t{"zero", 0.0, output_limiter_error_t::transition_half_integral_not_positive},
        invalid_half_integral_t{"negative", -0.1, output_limiter_error_t::transition_half_integral_not_positive}),
    test_name_generator_t<invalid_half_integral_t>{});

TEST_F(shaping_transforms_compact_output_limiter_test_t, rejects_nonfinite_derived_half_width)
{
    auto const tiny = std::numeric_limits<scalar_t>::denorm_min();
    EXPECT_CALL(mock_transition, antiderivative(scalar_t{0.5})).WillOnce(Return(tiny));
    auto const result = upper_t::make(bound, delta_y, transition_t{&mock_transition});
    EXPECT_EQ(result, std::unexpected{output_limiter_error_t::log_half_width_not_finite});
}

TEST_F(shaping_transforms_compact_output_limiter_test_t, rejects_positive_softness_that_rounds_to_zero_half_width)
{
    auto const huge = std::numeric_limits<scalar_t>::max();
    auto const tiny = std::numeric_limits<scalar_t>::denorm_min();
    EXPECT_CALL(mock_transition, antiderivative(scalar_t{0.5})).WillOnce(Return(half_integral));
    auto const result = lower_t::make(huge, tiny, transition_t{&mock_transition});
    EXPECT_EQ(result, std::unexpected{output_limiter_error_t::log_half_width_not_positive});
}

} // namespace
} // namespace crv::shaping::transforms
