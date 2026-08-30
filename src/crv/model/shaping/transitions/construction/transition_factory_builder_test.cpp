// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "transition_factory_builder.hpp"
#include <crv/quadrature/adaptive_integration_receipt.hpp>
#include <crv/quadrature/antiderivative_factory.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <optional>
#include <utility>

namespace crv::shaping::transitions::construction {
namespace {

struct transition_factory_builder_test_t : Test
{
    using scalar_t = float_t;
    using quadrature_receipt_t = quadrature::adaptive_integration_receipt_t<scalar_t>;

    struct antiderivative_t
    {
        [[nodiscard]] auto domain_end() const noexcept -> scalar_t { return scalar_t{0.5}; }
        [[nodiscard]] auto operator()(scalar_t) const noexcept -> scalar_t { return scalar_t{0}; }
    };

    struct integration_result_t
    {
        antiderivative_t antiderivative;
        quadrature_receipt_t receipt;
    };

    struct mock_antiderivative_factory_t
    {
        virtual ~mock_antiderivative_factory_t() = default;
        MOCK_METHOD(integration_result_t, call, (scalar_t domain_end, scalar_t tolerance, int_t depth_limit), (const));
    };
    StrictMock<mock_antiderivative_factory_t> mock_antiderivative_factory;

    struct antiderivative_factory_t
    {
        using scalar_t = transition_factory_builder_test_t::scalar_t;
        using receipt_t = quadrature_receipt_t;

        template <typename integrand_t> using antiderivative_t = transition_factory_builder_test_t::antiderivative_t;

        mock_antiderivative_factory_t* mock = nullptr;

        template <typename integrand_t>
        auto operator()(integrand_t, scalar_t domain_end, scalar_t tolerance, auto const&, int_t depth_limit) const
            -> integration_result_t
        {
            return mock->call(domain_end, tolerance, depth_limit);
        }
    };

    struct fixed_antiderivative_factory_t
    {
        using scalar_t = transition_factory_builder_test_t::scalar_t;
        using receipt_t = quadrature_receipt_t;

        template <typename integrand_t> using antiderivative_t = transition_factory_builder_test_t::antiderivative_t;

        integration_result_t result;

        template <typename integrand_t>
        auto operator()(integrand_t, scalar_t, scalar_t, auto const&, int_t) const -> integration_result_t
        {
            return result;
        }
    };

    static constexpr auto tolerance = scalar_t{0.125};
    static constexpr auto depth_limit = int_t{37};

    using sut_t = transition_factory_builder_t<antiderivative_factory_t>;
    using assertion_sut_t = transition_factory_builder_t<fixed_antiderivative_factory_t>;
    sut_t sut{antiderivative_factory_t{&mock_antiderivative_factory}, tolerance, depth_limit};

    static auto assertion_sut(quadrature_receipt_t receipt) -> assertion_sut_t
    {
        return assertion_sut_t{fixed_antiderivative_factory_t{
                                   .result = {.antiderivative = {}, .receipt = receipt},
                               },
            tolerance, depth_limit};
    }

    auto expect_integration(quadrature_receipt_t receipt) -> void
    {
        EXPECT_CALL(mock_antiderivative_factory, call(scalar_t{0.5}, tolerance, depth_limit))
            .WillOnce(Return(integration_result_t{.antiderivative = {}, .receipt = receipt}));
    }
};

TEST_F(transition_factory_builder_test_t, forwards_quadrature_receipt_unchanged)
{
    auto const receipt = quadrature_receipt_t{
        .requested_tolerance = tolerance,
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

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG
TEST_F(transition_factory_builder_test_t, asserts_refinement_limited_integration)
{
    auto const sut = assertion_sut({
        .requested_tolerance = tolerance,
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
        .requested_tolerance = tolerance,
        .achieved_error = 0.25,
        .max_error = 0.25,
        .segment_count = 1,
        .refinement_limited = false,
    });

    EXPECT_DEATH(static_cast<void>(sut()), "missed requested tolerance");
}

#endif // defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct transition_factory_builder_production_test_t : Test
{
    using scalar_t = float_t;
    using antiderivative_factory_t = quadrature::antiderivative_factory_t<scalar_t>;
    using sut_t = transition_factory_builder_t<antiderivative_factory_t>;
    using factory_t = sut_t::factory_t;

    static constexpr auto requested_tolerance = scalar_t{1e-12};
    static constexpr auto depth_limit = int_t{32};

    sut_t sut{antiderivative_factory_t{}, requested_tolerance, depth_limit};
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
