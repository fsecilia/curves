// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "interval.hpp"
#include <crv/spline/construction/error.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <gmock/gmock.h>
#include <queue>
#include <vector>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;

namespace interval_priority_less_tests {

using x_t = fixed_t<int_t, 0>;

struct subdomain_t
{
    using scalar_t = float_t;
    x_t left_x;
};

using segment_t = int_t;
using sut_t = interval_t<subdomain_t, segment_t>;

constexpr auto construct_sut(scalar_t weighted_error, x_t left_x) noexcept -> sut_t
{
    auto result = sut_t{};
    result.subdomain.left_x = left_x;
    result.residual.emplace();
    result.residual->weighted_error = weighted_error;
    return result;
}

constexpr auto sut = interval_priority_less_t{};

static_assert(sut(construct_sut(0.0, x_t{100}), construct_sut(1.0, x_t{0})));
static_assert(sut(construct_sut(5.0, x_t{1}), construct_sut(5.0, x_t{2})));
static_assert(!sut(construct_sut(5.0, x_t{2}), construct_sut(5.0, x_t{1})));

constexpr auto construct_required(x_t left_x) noexcept -> sut_t
{
    auto result = sut_t{};
    result.subdomain.left_x = left_x;
    return result;
}

static_assert(sut(construct_sut(100.0, x_t{0}), construct_required(x_t{1})));
static_assert(!sut(construct_required(x_t{1}), construct_sut(100.0, x_t{0})));

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(spline_interval_priority_less, death_by_nonfinite_weighted_error)
{
    auto const nan = std::numeric_limits<scalar_t>::quiet_NaN();
    EXPECT_DEBUG_DEATH(sut(construct_sut(nan, x_t{0}), construct_sut(0.0, x_t{0})), "isfinite");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace interval_priority_less_tests

namespace interval_factory_tests {

struct spline_interval_factory_test_t : Test
{
    using x_t = fixed_t<int64_t, 2>;
    using y_t = fixed_t<int64_t, 2>;
    using subdomain_t = spline::subdomain_t<scalar_t, x_t>;
    using function_sample_t = subdomain_t::function_sample_t;
    using cubic_t = cubic_t<scalar_t>;

    int_t safety_calls = 0;
    int_t endpoint_evaluation_calls = 0;

    struct unpacked_segment_t
    {
        int_t id;
        constexpr auto operator==(unpacked_segment_t const&) const noexcept -> bool = default;
    };

    struct segment_t
    {
        using y_t = spline_interval_factory_test_t::y_t;

        unpacked_segment_t unpacked;
        cubic_t cubic;
        scalar_t left_endpoint_derivative;
        x_t width;
        x_t x0;
        y_t anchor;
        bool safe;
        int_t* safety_calls;
        int_t* endpoint_evaluation_calls;

        constexpr auto unpacked_segment() const noexcept -> unpacked_segment_t { return unpacked; }

        auto operator()(x_t x, x_t passed_x0) const noexcept -> y_t
        {
            ++*endpoint_evaluation_calls;
            assert(x == x0 + width);
            assert(passed_x0 == x0);
            return anchor;
        }

        auto is_safe_through(x_t u_max, x_t passed_x0) const noexcept -> bool
        {
            ++*safety_calls;
            return safe && u_max == width && passed_x0 == x0;
        }

        constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
    };

    struct segment_factory_t
    {
        using segment_t = spline_interval_factory_test_t::segment_t;
        using error_t = spline_construction_error_t<x_t>;
        using result_t = std::expected<segment_t, error_t>;

        unpacked_segment_t unpacked_segment{};
        y_t anchor{};
        bool safe = true;
        int_t* safety_calls = nullptr;
        int_t* endpoint_evaluation_calls = nullptr;
        std::optional<error_t> error;

        auto operator()(cubic_t const& cubic, scalar_t left_endpoint_derivative, x_t width, x_t x0) const noexcept
            -> result_t
        {
            if (error) return std::unexpected{*error};
            return segment_t{
                .unpacked = unpacked_segment,
                .cubic = cubic,
                .left_endpoint_derivative = left_endpoint_derivative,
                .width = width,
                .x0 = x0,
                .anchor = anchor,
                .safe = safe,
                .safety_calls = safety_calls,
                .endpoint_evaluation_calls = endpoint_evaluation_calls,
            };
        }
    };

    struct mock_right_gain_slope_calculator_t
    {
        virtual ~mock_right_gain_slope_calculator_t() = default;
        MOCK_METHOD(scalar_t, call, (unpacked_segment_t unpacked_segment, x_t width, x_t x0), (const, noexcept));
    };
    StrictMock<mock_right_gain_slope_calculator_t> mock_right_gain_slope_calculator;

    struct right_gain_slope_calculator_t
    {
        mock_right_gain_slope_calculator_t* mock = nullptr;
        auto operator()(unpacked_segment_t unpacked_segment, x_t width, x_t x0) const noexcept -> scalar_t
        {
            return mock->call(unpacked_segment, width, x0);
        }
    };

    struct mock_final_endpoint_acceptance_t
    {
        virtual ~mock_final_endpoint_acceptance_t() = default;
        MOCK_METHOD(bool, call, (y_t anchor, scalar_t right_gain_slope, y_t y_limit), (const, noexcept));
    };
    StrictMock<mock_final_endpoint_acceptance_t> mock_final_endpoint_acceptance;

    struct final_endpoint_acceptance_t
    {
        mock_final_endpoint_acceptance_t* mock = nullptr;
        auto operator()(y_t anchor, scalar_t right_gain_slope, y_t y_limit) const noexcept -> bool
        {
            return mock->call(anchor, right_gain_slope, y_limit);
        }
    };

    struct approximant_t
    {
        using x_t = spline_interval_factory_test_t::x_t;

        segment_t segment;
        x_t x0;

        constexpr auto operator==(approximant_t const&) const noexcept -> bool = default;
    };

    struct approximant_factory_t
    {
        using approximant_t = approximant_t;

        constexpr auto operator()(segment_t const& segment, x_t x0) const noexcept -> approximant_t
        {
            return {segment, x0};
        }
    };

    struct mock_hermite_converter_t
    {
        virtual ~mock_hermite_converter_t() = default;
        MOCK_METHOD(cubic_t, call, (jet_t left, jet_t right), (const, noexcept));
    };
    StrictMock<mock_hermite_converter_t> mock_hermite_converter;

    struct hermite_converter_t
    {
        mock_hermite_converter_t* mock = nullptr;
        auto operator()(jet_t left_y, jet_t right_y) const noexcept -> cubic_t { return mock->call(left_y, right_y); }
    };

    struct mock_local_coordinate_converter_t
    {
        virtual ~mock_local_coordinate_converter_t() = default;
        MOCK_METHOD(cubic_t, call, (cubic_t const& normalized, scalar_t width), (const, noexcept));
    };
    StrictMock<mock_local_coordinate_converter_t> mock_local_coordinate_converter;

    struct local_coordinate_converter_t
    {
        mock_local_coordinate_converter_t* mock = nullptr;
        auto operator()(cubic_t const& normalized, scalar_t width) const noexcept -> cubic_t
        {
            return mock->call(normalized, width);
        }
    };

    struct residual_t
    {
        int_t id = 0;
        scalar_t weighted_error = 0;
        auto operator==(residual_t const&) const noexcept -> bool = default;
    };

    struct sample_target_function_t
    {
        int_t id = 0;
        constexpr auto operator==(sample_target_function_t const&) const noexcept -> bool = default;
    };

    struct mock_residual_estimator_t
    {
        virtual ~mock_residual_estimator_t() = default;
        MOCK_METHOD(residual_t, call,
            (sample_target_function_t sample_target_function, approximant_t approximant, scalar_t left_x,
                scalar_t midpoint_x, scalar_t right_x),
            (const, noexcept));
    };
    StrictMock<mock_residual_estimator_t> mock_residual_estimator;

    struct residual_estimator_t
    {
        mock_residual_estimator_t* mock = nullptr;
        auto operator()(sample_target_function_t sample_target_function, approximant_t approximant, scalar_t left_x,
            scalar_t midpoint_x, scalar_t right_x) const noexcept -> residual_t
        {
            return mock->call(sample_target_function, approximant, left_x, midpoint_x, right_x);
        }
    };

    struct interval_t
    {
        using scalar_t = float_t;
        using segment_t = spline_interval_factory_test_t::segment_t;
        using subdomain_t = spline_interval_factory_test_t::subdomain_t;

        segment_t segment;
        scalar_t right_gain_slope;
        subdomain_t subdomain;
        std::optional<residual_t> residual;

        constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    static constexpr auto domain_end = x_t{4};
    static constexpr auto y_limit = y_t{1000};

    using sut_t = spline::interval_factory_t<interval_t, segment_factory_t, right_gain_slope_calculator_t,
        final_endpoint_acceptance_t, approximant_factory_t, hermite_converter_t, local_coordinate_converter_t,
        residual_estimator_t, domain_end, y_limit>;
    sut_t sut{
        .segment_factory = {
            .unpacked_segment = {91},
            .anchor = y_t{5},
            .safe = true,
            .safety_calls = &safety_calls,
            .endpoint_evaluation_calls = &endpoint_evaluation_calls,
            .error = std::nullopt,
        },
        .calc_right_gain_slope = right_gain_slope_calculator_t{&mock_right_gain_slope_calculator},
        .accept_final_endpoint = final_endpoint_acceptance_t{&mock_final_endpoint_acceptance},
        .approximant_factory = {},
        .convert_hermite = hermite_converter_t{&mock_hermite_converter},
        .convert_local_coordinate = local_coordinate_converter_t{&mock_local_coordinate_converter},
        .estimate_residual = residual_estimator_t{&mock_residual_estimator},
    };

    sample_target_function_t const sample_target_function{1};
    x_t const left_x = x_t::literal(8); // 2.0
    x_t const midpoint_x = x_t::literal(10); // 2.5
    x_t const right_x = x_t::literal(13); // 3.25, odd raw width = 5
    function_sample_t const left{.x = 2.0, .y = {3.0, 4.0}};
    function_sample_t const midpoint{.x = 2.5, .y = {6.0, 7.0}};
    function_sample_t const right{.x = 3.25, .y = {9.0, 10.0}};
    subdomain_t const subdomain{
        .left_x = left_x,
        .midpoint_x = midpoint_x,
        .right_x = right_x,
        .left = left,
        .midpoint = midpoint,
        .right = right,
    };

    scalar_t const width = 1.25;
    x_t const width_fixed = x_t::literal(5);
    cubic_t const normalized_cubic{1.0, 2.0, 3.0, 4.0};
    cubic_t const local_cubic{10.0, 20.0, 30.0, 40.0};
    unpacked_segment_t const unpacked_segment{91};
    segment_t const segment{
        .unpacked = unpacked_segment,
        .cubic = local_cubic,
        .left_endpoint_derivative = left.y.df,
        .width = width_fixed,
        .x0 = left_x,
        .anchor = y_t{5},
        .safe = true,
        .safety_calls = &safety_calls,
        .endpoint_evaluation_calls = &endpoint_evaluation_calls,
    };
    scalar_t const right_gain_slope = -2.75;
    residual_t const residual{.id = 14, .weighted_error = 17.0};

    auto make_final_subdomain() const -> subdomain_t
    {
        return {
            .left_x = left_x,
            .midpoint_x = x_t{3},
            .right_x = domain_end,
            .left = left,
            .midpoint = function_sample_t{.x = 3.0, .y = {6.0, 7.0}},
            .right = function_sample_t{.x = 4.0, .y = {9.0, 10.0}},
        };
    }

    auto make_segment(x_t passed_width) -> segment_t
    {
        return {
            .unpacked = unpacked_segment,
            .cubic = local_cubic,
            .left_endpoint_derivative = left.y.df,
            .width = passed_width,
            .x0 = left_x,
            .anchor = sut.segment_factory.anchor,
            .safe = sut.segment_factory.safe,
            .safety_calls = &safety_calls,
            .endpoint_evaluation_calls = &endpoint_evaluation_calls,
        };
    }
};

TEST_F(spline_interval_factory_test_t, runtime_safe_interior_negative_slope_measures_the_same_fixed_segment_it_stores)
{
    auto local_left_y = left.y;
    auto local_right_y = right.y;
    local_left_y.df *= width;
    local_right_y.df *= width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, width)).WillOnce(Return(local_cubic));
    EXPECT_CALL(mock_right_gain_slope_calculator, call(unpacked_segment, width_fixed, left_x))
        .WillOnce(Return(right_gain_slope));
    EXPECT_CALL(mock_residual_estimator,
        call(sample_target_function, approximant_t{.segment = segment, .x0 = left_x}, left.x, midpoint.x, right.x))
        .WillOnce(Return(residual));

    auto const actual = sut(sample_target_function, subdomain);

    auto const expected = interval_t{
        .segment = segment,
        .right_gain_slope = right_gain_slope,
        .subdomain = subdomain,
        .residual = residual,
    };
    ASSERT_TRUE(actual);
    EXPECT_EQ(expected, *actual);
}

TEST_F(spline_interval_factory_test_t, runtime_unsafe_final_segment_skips_endpoint_acceptance_and_residual)
{
    sut.segment_factory.safe = false;
    auto const final_subdomain = make_final_subdomain();
    auto const final_width_fixed = final_subdomain.width();
    auto const final_width = from_fixed<scalar_t>(final_width_fixed);

    auto local_left_y = final_subdomain.left.y;
    auto local_right_y = final_subdomain.right.y;
    local_left_y.df *= final_width;
    local_right_y.df *= final_width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, final_width)).WillOnce(Return(local_cubic));
    EXPECT_CALL(mock_right_gain_slope_calculator, call(unpacked_segment, final_width_fixed, left_x))
        .WillOnce(Return(right_gain_slope));

    auto const actual = sut(sample_target_function, final_subdomain);

    ASSERT_TRUE(actual);
    EXPECT_FALSE(actual->residual.has_value());
    EXPECT_FALSE(actual->segment.safe);
    EXPECT_EQ(actual->right_gain_slope, right_gain_slope);
    EXPECT_EQ(endpoint_evaluation_calls, 0);
}

TEST_F(spline_interval_factory_test_t, accepted_final_encoded_endpoint_is_measured_for_residual)
{
    auto const final_subdomain = make_final_subdomain();
    auto const final_width_fixed = final_subdomain.width();
    auto const final_width = from_fixed<scalar_t>(final_width_fixed);
    auto const final_segment = make_segment(final_width_fixed);
    auto const accepted_slope = scalar_t{1.5};

    auto local_left_y = final_subdomain.left.y;
    auto local_right_y = final_subdomain.right.y;
    local_left_y.df *= final_width;
    local_right_y.df *= final_width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, final_width)).WillOnce(Return(local_cubic));
    EXPECT_CALL(mock_right_gain_slope_calculator, call(unpacked_segment, final_width_fixed, left_x))
        .WillOnce(Return(accepted_slope));
    EXPECT_CALL(mock_final_endpoint_acceptance, call(final_segment.anchor, accepted_slope, y_limit))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_residual_estimator,
        call(sample_target_function, approximant_t{.segment = final_segment, .x0 = left_x}, final_subdomain.left.x,
            final_subdomain.midpoint.x, final_subdomain.right.x))
        .WillOnce(Return(residual));

    auto const actual = sut(sample_target_function, final_subdomain);

    ASSERT_TRUE(actual);
    EXPECT_EQ(actual->residual, residual);
    EXPECT_EQ(endpoint_evaluation_calls, 1);
}

TEST_F(spline_interval_factory_test_t, packed_endpoint_anchor_controls_final_requiredness)
{
    sut.segment_factory.anchor = y_t{-1};
    auto const final_subdomain = make_final_subdomain();
    auto const final_width_fixed = final_subdomain.width();
    auto const final_width = from_fixed<scalar_t>(final_width_fixed);
    auto const represented_slope = scalar_t{1.5};

    auto local_left_y = final_subdomain.left.y;
    auto local_right_y = final_subdomain.right.y;
    local_left_y.df *= final_width;
    local_right_y.df *= final_width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, final_width)).WillOnce(Return(local_cubic));
    EXPECT_CALL(mock_right_gain_slope_calculator, call(unpacked_segment, final_width_fixed, left_x))
        .WillOnce(Return(represented_slope));
    EXPECT_CALL(mock_final_endpoint_acceptance, call(y_t{-1}, represented_slope, y_limit)).WillOnce(Return(false));

    auto const actual = sut(sample_target_function, final_subdomain);

    ASSERT_TRUE(actual);
    EXPECT_FALSE(actual->residual.has_value());
    EXPECT_EQ(actual->segment.anchor, y_t{-1});
    EXPECT_EQ(final_subdomain.right.y.f, scalar_t{9});
    EXPECT_EQ(endpoint_evaluation_calls, 1);
}

TEST_F(spline_interval_factory_test_t, represented_right_gain_slope_controls_final_requiredness)
{
    auto const final_subdomain = make_final_subdomain();
    auto const final_width_fixed = final_subdomain.width();
    auto const final_width = from_fixed<scalar_t>(final_width_fixed);
    auto const represented_slope = scalar_t{-1.5};

    auto local_left_y = final_subdomain.left.y;
    auto local_right_y = final_subdomain.right.y;
    local_left_y.df *= final_width;
    local_right_y.df *= final_width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, final_width)).WillOnce(Return(local_cubic));
    EXPECT_CALL(mock_right_gain_slope_calculator, call(unpacked_segment, final_width_fixed, left_x))
        .WillOnce(Return(represented_slope));
    EXPECT_CALL(mock_final_endpoint_acceptance, call(y_t{5}, represented_slope, y_limit)).WillOnce(Return(false));

    auto const actual = sut(sample_target_function, final_subdomain);

    auto const target_gain = final_subdomain.right.y.f / final_subdomain.right.x;
    auto const target_gain_slope = (final_subdomain.right.y.df - target_gain) / final_subdomain.right.x;
    ASSERT_GT(target_gain_slope, scalar_t{0});
    ASSERT_TRUE(actual);
    EXPECT_FALSE(actual->residual.has_value());
    EXPECT_EQ(actual->right_gain_slope, represented_slope);
}

TEST_F(spline_interval_factory_test_t, endpoint_required_final_interval_outranks_large_optional_interior_residual)
{
    sut.segment_factory.anchor = y_t{-1};
    auto const final_subdomain = make_final_subdomain();
    auto const final_width_fixed = final_subdomain.width();
    auto const final_width = from_fixed<scalar_t>(final_width_fixed);
    auto const represented_slope = scalar_t{1.5};

    auto local_left_y = final_subdomain.left.y;
    auto local_right_y = final_subdomain.right.y;
    local_left_y.df *= final_width;
    local_right_y.df *= final_width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, final_width)).WillOnce(Return(local_cubic));
    EXPECT_CALL(mock_right_gain_slope_calculator, call(unpacked_segment, final_width_fixed, left_x))
        .WillOnce(Return(represented_slope));
    EXPECT_CALL(mock_final_endpoint_acceptance, call(y_t{-1}, represented_slope, y_limit)).WillOnce(Return(false));

    auto const mandatory_final = sut(sample_target_function, final_subdomain);
    ASSERT_TRUE(mandatory_final);
    ASSERT_FALSE(mandatory_final->residual.has_value());
    ASSERT_TRUE(mandatory_final->segment.safe);

    auto const optional_interior = interval_t{
        .segment = segment,
        .right_gain_slope = right_gain_slope,
        .subdomain = subdomain,
        .residual = residual_t{.id = 99, .weighted_error = 1e9},
    };
    auto pool = std::priority_queue<interval_t, std::vector<interval_t>, interval_priority_less_t>{};
    pool.push(optional_interior);
    pool.push(*mandatory_final);

    EXPECT_FALSE(pool.top().residual.has_value());
    EXPECT_EQ(pool.top().subdomain.right_x, domain_end);
}

TEST_F(spline_interval_factory_test_t, segment_construction_error_skips_runtime_safety_and_residual)
{
    auto const failure = segment_factory_t::error_t{
        .reason = spline_construction_error_reason_t::gain_anchor_not_representable,
        .left = left_x,
        .right = right_x,
    };
    sut.segment_factory.error = failure;

    auto local_left_y = left.y;
    auto local_right_y = right.y;
    local_left_y.df *= width;
    local_right_y.df *= width;

    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(normalized_cubic));
    EXPECT_CALL(mock_local_coordinate_converter, call(normalized_cubic, width)).WillOnce(Return(local_cubic));

    auto const actual = sut(sample_target_function, subdomain);

    ASSERT_FALSE(actual);
    EXPECT_EQ(actual.error(), failure);
    EXPECT_EQ(safety_calls, 0);
}

} // namespace interval_factory_tests

} // namespace
} // namespace crv::spline
