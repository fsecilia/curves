// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "adaptive_integrator.hpp"
#include <crv/quadrature/antiderivative_cache.hpp>
#include <crv/quadrature/rules.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature::construction {
namespace {

struct quadrature_adaptive_integrator_test_t : Test
{
    using scalar_t = float_t;
    using segment_t = construction::segment_t<scalar_t>;
    using rule_t = rules::gauss_kronrod_t<scalar_t>;
    using stack_t = std::vector<segment_t>;
    using critical_points_t = std::vector<scalar_t>;
    using cache_t = quadrature::antiderivative_cache_t<scalar_t>;

    static constexpr auto integrand = [](scalar_t x) static noexcept -> scalar_t { return x * x; };
    using integrand_t = decltype(integrand);

    struct integral_t
    {
        using scalar_t = quadrature_adaptive_integrator_test_t::scalar_t;

        integrand_t integrand;
        rule_t rule;

        auto operator==(integral_t const&) const noexcept -> bool = default;
    };

    struct cache_builder_result_t
    {
        cache_t cache;
        scalar_t achieved_error;
        scalar_t max_error;
        bool refinement_limited;
    };

    struct mock_cache_builder_t
    {
        virtual ~mock_cache_builder_t() = default;

        MOCK_METHOD(cache_builder_result_t, finalize, (), (noexcept));
    };
    StrictMock<mock_cache_builder_t> mock_cache_builder;

    struct cache_builder_t
    {
        using result_t = cache_builder_result_t;

        mock_cache_builder_t* mock = nullptr;

        auto finalize() && noexcept -> result_t { return mock->finalize(); }
    };

    struct mock_cache_builder_factory_t
    {
        virtual ~mock_cache_builder_factory_t() = default;

        MOCK_METHOD(void, call, (), (const));
    };
    StrictMock<mock_cache_builder_factory_t> mock_cache_builder_factory;

    struct cache_builder_factory_t
    {
        mock_cache_builder_factory_t* factory = nullptr;
        mock_cache_builder_t* builder = nullptr;

        auto operator()() const -> cache_builder_t
        {
            factory->call();
            return {.mock = builder};
        }
    };

    struct mock_subdivider_t
    {
        virtual ~mock_subdivider_t() = default;

        MOCK_METHOD(void, run,
            (stack_t & stack, integral_t const& integral, mock_cache_builder_t& cache_builder, int_t depth_limit),
            (const));
    };
    StrictMock<mock_subdivider_t> mock_subdivider;

    struct subdivider_t
    {
        mock_subdivider_t* mock = nullptr;

        auto run(stack_t& stack, integral_t const& integral, cache_builder_t& cache_builder, int_t depth_limit) const
            -> void
        {
            mock->run(stack, integral, *cache_builder.mock, depth_limit);
        }
    };

    struct mock_stack_seeder_t
    {
        virtual ~mock_stack_seeder_t() = default;

        MOCK_METHOD(void, seed,
            (stack_t & stack, integral_t const& integral, scalar_t domain_end, scalar_t global_tolerance,
                critical_points_t const& critical_points),
            (const));
    };
    StrictMock<mock_stack_seeder_t> mock_stack_seeder;

    struct stack_seeder_t
    {
        mock_stack_seeder_t* mock = nullptr;

        auto seed(stack_t& stack, integral_t const& integral, scalar_t domain_end, scalar_t global_tolerance,
            critical_points_t const& critical_points) const -> void
        {
            mock->seed(stack, integral, domain_end, global_tolerance, critical_points);
        }
    };

    static constexpr auto tolerance = scalar_t{1e-12};
    static constexpr auto depth_limit = int_t{64};
    static constexpr auto domain_end = scalar_t{256};
    static constexpr auto integral = integral_t{integrand, rule_t{}};
    static constexpr auto achieved_error = scalar_t{1e-13};
    static constexpr auto max_error = scalar_t{5e-14};

    using sut_t = adaptive_integrator_t<scalar_t, cache_builder_factory_t, subdivider_t, stack_seeder_t>;
    sut_t sut{cache_builder_factory_t{.factory = &mock_cache_builder_factory, .builder = &mock_cache_builder},
        subdivider_t{&mock_subdivider}, stack_seeder_t{&mock_stack_seeder}};

    auto expect_integration(critical_points_t const& critical_points) -> void
    {
        auto const seq = InSequence{};
        EXPECT_CALL(mock_cache_builder_factory, call());
        EXPECT_CALL(mock_stack_seeder, seed(_, integral, domain_end, tolerance, critical_points));
        EXPECT_CALL(mock_subdivider, run(_, integral, Ref(mock_cache_builder), depth_limit));
        EXPECT_CALL(mock_cache_builder, finalize())
            .WillOnce(Return(cache_builder_result_t{
                .cache = cache_t{{scalar_t{0}, domain_end}, {scalar_t{0}, scalar_t{1}}},
                .achieved_error = achieved_error,
                .max_error = max_error,
                .refinement_limited = false,
            }));
    }
};

TEST_F(quadrature_adaptive_integrator_test_t, orchestrates_dependencies_through_production_operator)
{
    auto const critical_points = critical_points_t{0.25, 0.33, 1.0};
    expect_integration(critical_points);

    auto const result = sut(integral, domain_end, tolerance, depth_limit, critical_points);

    EXPECT_EQ(result.antiderivative.domain_end(), domain_end);
}

TEST_F(quadrature_adaptive_integrator_test_t, receipt_retains_requested_tolerance)
{
    auto const critical_points = critical_points_t{0.25, 0.33, 1.0};
    expect_integration(critical_points);

    auto const result = sut(integral, domain_end, tolerance, depth_limit, critical_points);

    EXPECT_EQ(result.receipt.requested_tolerance, tolerance);
}

TEST_F(quadrature_adaptive_integrator_test_t, receipt_retains_segment_count)
{
    auto const critical_points = critical_points_t{0.25, 0.33, 1.0};
    expect_integration(critical_points);

    auto const result = sut(integral, domain_end, tolerance, depth_limit, critical_points);

    EXPECT_EQ(result.receipt.segment_count, int_t{1});
}

} // namespace
} // namespace crv::quadrature::construction
