// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "transition_factory_builder.hpp"
#include <crv/quadrature/construction/adaptive_integrator.hpp>
#include <crv/quadrature/construction/antiderivative_cache_builder.hpp>
#include <crv/quadrature/construction/stack_seeder.hpp>
#include <crv/quadrature/construction/subdivider.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <optional>
#include <utility>

namespace crv::shaping::transitions::construction {
namespace {

struct transition_factory_builder_test_t : Test
{
    using scalar_t = float_t;

    struct antiderivative_t
    {
        [[nodiscard]] auto domain_end() const noexcept -> scalar_t { return scalar_t{0.5}; }
        [[nodiscard]] auto operator()(scalar_t) const noexcept -> scalar_t { return scalar_t{0}; }
    };

    struct quadrature_receipt_t
    {
        scalar_t requested_tolerance;
        scalar_t achieved_error;
        scalar_t max_error;
        int_t segment_count;
        bool refinement_limited;

        constexpr auto operator==(quadrature_receipt_t const&) const noexcept -> bool = default;
    };

    struct integration_result_t
    {
        using antiderivative_t = transition_factory_builder_test_t::antiderivative_t;
        using receipt_t = quadrature_receipt_t;

        antiderivative_t antiderivative;
        receipt_t receipt;
    };

    struct mock_integrator_t
    {
        virtual ~mock_integrator_t() = default;
        MOCK_METHOD(integration_result_t, call, (scalar_t domain_end), (const));
    };
    StrictMock<mock_integrator_t> mock_integrator;

    struct integrator_t
    {
        mock_integrator_t* mock = nullptr;

        template <typename integral_t>
        auto operator()(integral_t, scalar_t domain_end, std::array<scalar_t, 0> const&) const -> integration_result_t
        {
            return mock->call(domain_end);
        }
    };

    struct fixed_integrator_t
    {
        integration_result_t result;

        template <typename integral_t>
        auto operator()(integral_t, scalar_t, std::array<scalar_t, 0> const&) const -> integration_result_t
        {
            return result;
        }
    };

    using sut_t = transition_factory_builder_t<scalar_t, integrator_t>;
    using assertion_sut_t = transition_factory_builder_t<scalar_t, fixed_integrator_t>;
    sut_t sut{integrator_t{&mock_integrator}};

    static auto assertion_sut(quadrature_receipt_t receipt) -> assertion_sut_t
    {
        return assertion_sut_t{fixed_integrator_t{
            .result = {.antiderivative = {}, .receipt = receipt},
        }};
    }

    auto expect_integration(quadrature_receipt_t receipt) -> void
    {
        EXPECT_CALL(mock_integrator, call(scalar_t{0.5}))
            .WillOnce(Return(integration_result_t{.antiderivative = {}, .receipt = receipt}));
    }
};

TEST_F(transition_factory_builder_test_t, forwards_quadrature_receipt_unchanged)
{
    auto const receipt = quadrature_receipt_t{
        .requested_tolerance = 0.125,
        .achieved_error = 0.0625,
        .max_error = 0.03125,
        .segment_count = 53,
        .refinement_limited = false,
    };
    expect_integration(receipt);

    auto const factory = sut();
    auto const actual = factory(
        continuity_t::cinfinity, []<typename product_t>(product_t product) -> std::optional<quadrature_receipt_t> {
            return product.quadrature_receipt;
        });

    EXPECT_EQ(actual, receipt);
}

TEST_F(transition_factory_builder_test_t, asserts_refinement_limited_integration)
{
    auto const sut = assertion_sut({
        .requested_tolerance = 0.125,
        .achieved_error = 0.0625,
        .max_error = 0.03125,
        .segment_count = 1,
        .refinement_limited = true,
    });

    EXPECT_DEATH(static_cast<void>(sut()), "refinement limited");
}

TEST_F(transition_factory_builder_test_t, asserts_error_above_requested_tolerance)
{
    auto const sut = assertion_sut({
        .requested_tolerance = 0.125,
        .achieved_error = 0.25,
        .max_error = 0.25,
        .segment_count = 1,
        .refinement_limited = false,
    });

    EXPECT_DEATH(static_cast<void>(sut()), "missed requested tolerance");
}

struct transition_factory_builder_production_test_t : Test
{
    using scalar_t = float_t;
    using cache_builder_factory_t = quadrature::construction::antiderivative_cache_builder_factory_t<scalar_t>;
    using subdivider_t = quadrature::construction::subdivider_t<scalar_t>;
    using stack_seeder_t = quadrature::construction::stack_seeder_t<scalar_t>;
    using integrator_t = quadrature::construction::adaptive_integrator_t<scalar_t, cache_builder_factory_t,
        subdivider_t, stack_seeder_t>;
    using sut_t = transition_factory_builder_t<scalar_t, integrator_t>;

    static constexpr auto requested_tolerance = scalar_t{1e-12};
    static constexpr auto depth_limit = int_t{32};

    using factory_t = decltype(std::declval<sut_t const&>()());

    sut_t sut{integrator_t{requested_tolerance, depth_limit}};
    factory_t factory = sut();
};

TEST_F(transition_factory_builder_production_test_t, nast_meets_requested_quadrature_tolerance)
{
    auto const receipt = factory(continuity_t::cinfinity,
        []<typename product_t>(product_t product) -> std::optional<typename product_t::quadrature_receipt_t> {
            return product.quadrature_receipt;
        });

    EXPECT_LE(receipt->achieved_error, requested_tolerance);
}

TEST_F(transition_factory_builder_production_test_t, constructs_accurate_nast_antiderivative)
{
    auto const actual = factory(continuity_t::cinfinity,
        []<typename product_t>(product_t product) -> scalar_t { return product.transition.antiderivative(0.5); });

    EXPECT_NEAR(actual, 0.0688874741344636, 2e-14);
}

} // namespace
} // namespace crv::shaping::transitions::construction
