// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "adaptive_integrator.hpp"
#include <crv/math/quadrature/rules.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature::generic {
namespace {

struct quadrature_adaptive_integrator_test : Test
{
    using real_t        = float_t;
    using segment_t     = segment_t<real_t>;
    using accumulator_t = real_t;
    using rule_t        = rules::gauss_kronrod_t<real_t, 15>;

    using stack_t           = std::vector<segment_t>;
    using critical_points_t = std::vector<real_t>;

    static constexpr auto integrand = [](real_t x) static noexcept -> real_t { return x * x; };
    using integrand_t               = decltype(integrand);

    struct integral_t
    {
        integrand_t integrand;
        rule_t      rule;

        auto operator==(integral_t const&) const noexcept -> bool = default;
    };

    struct mock_bisector_t
    {
        virtual ~mock_bisector_t() = default;

        MOCK_METHOD(integral_t, finalize, (), (noexcept));
    };
    StrictMock<mock_bisector_t> mock_bisector;

    struct bisector_t
    {
        mock_bisector_t* mock = nullptr;

        auto finalize() && -> integral_t { return mock->finalize(); }
    };

    struct antiderivative_t
    {
        using real_t = float_t;
        int_t id;

        auto operator==(antiderivative_t const&) const noexcept -> bool = default;
    };
    static constexpr auto antiderivative = antiderivative_t{371};

    struct mock_antiderivative_builder_t
    {
        virtual ~mock_antiderivative_builder_t() = default;

        MOCK_METHOD((integration_result_t<antiderivative_t>), finalize, (integral_t), (noexcept));
    };
    StrictMock<mock_antiderivative_builder_t> mock_antiderivative_builder;

    struct antiderivative_builder_t
    {
        using result_t = integration_result_t<antiderivative_t>;

        mock_antiderivative_builder_t* mock = nullptr;

        auto finalize(integral_t integral) && -> result_t { return mock->finalize(integral); }
    };

    struct mock_subdivider_t
    {
        virtual ~mock_subdivider_t() = default;

        MOCK_METHOD(void, run,
                    (stack_t & stack, mock_bisector_t const& bisector,
                     mock_antiderivative_builder_t& antiderivative_builder, int_t depth_limit),
                    (const));
    };
    StrictMock<mock_subdivider_t> mock_subdivider;

    struct subdivider_t
    {
        mock_subdivider_t* mock = nullptr;

        auto run(stack_t& stack, bisector_t const& bisector, antiderivative_builder_t& antiderivative_builder,
                 int_t depth_limit) -> void
        {
            mock->run(stack, *bisector.mock, *antiderivative_builder.mock, depth_limit);
        }
    };

    struct mock_stack_seeder_t
    {
        virtual ~mock_stack_seeder_t() = default;

        MOCK_METHOD(void, seed,
                    (stack_t & stack, mock_bisector_t const& bisector, real_t domain_max, real_t global_tolerance,
                     critical_points_t const& critical_points),
                    (const));
    };
    StrictMock<mock_stack_seeder_t> mock_stack_seeder;

    struct stack_seeder_t
    {
        mock_stack_seeder_t* mock = nullptr;

        auto seed(stack_t& stack, bisector_t const& bisector, real_t domain_max, real_t global_tolerance,
                  critical_points_t const& critical_points) -> void
        {
            mock->seed(stack, *bisector.mock, domain_max, global_tolerance, critical_points);
        }
    };

    static constexpr auto tolerance      = 1e-12;
    static constexpr auto depth_limit    = 64;
    static constexpr auto domain_max     = 256;
    static constexpr auto integral       = integral_t{integrand, rule_t{}};
    static constexpr auto achieved_error = 3.25e-10;
    static constexpr auto max_error      = 6.98e-8;

    using sut_t = adaptive_integrator_t<real_t, accumulator_t, subdivider_t, stack_seeder_t>;
    sut_t sut{tolerance, depth_limit, subdivider_t{&mock_subdivider}, stack_seeder_t{&mock_stack_seeder}};
};

TEST_F(quadrature_adaptive_integrator_test, orchestrates_dependencies)
{
    using result_t = integration_result_t<antiderivative_t>;

    auto const seq = InSequence{};

    auto const critical_points = critical_points_t{0.25, 0.33, 1.0};
    EXPECT_CALL(mock_stack_seeder, seed(_, Ref(mock_bisector), domain_max, tolerance, critical_points));
    EXPECT_CALL(mock_subdivider, run(_, Ref(mock_bisector), Ref(mock_antiderivative_builder), depth_limit));
    EXPECT_CALL(mock_bisector, finalize()).WillOnce(Return(integral));

    auto const expected_result
        = result_t{.antiderivative = antiderivative, .achieved_error = achieved_error, .max_error = max_error};
    EXPECT_CALL(mock_antiderivative_builder, finalize(integral)).WillOnce(Return(expected_result));

    [[maybe_unused]] result_t actual_result
        = sut(bisector_t{&mock_bisector}, antiderivative_builder_t{&mock_antiderivative_builder}, domain_max,
              critical_points);

    EXPECT_EQ(expected_result, actual_result);
}

} // namespace
} // namespace crv::quadrature::generic
