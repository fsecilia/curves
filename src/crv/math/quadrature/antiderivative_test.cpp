// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "antiderivative.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature {
namespace {

// ====================================================================================================================
// antiderivative_t
// ====================================================================================================================

struct quadrature_antiderivative_test_t : Test
{
    using real_t = float_t;

    struct mock_integrand_t
    {
        MOCK_METHOD(real_t, call, (real_t location), (const, noexcept));
        virtual ~mock_integrand_t() = default;
    };
    StrictMock<mock_integrand_t> mock_integrand;

    struct integrand_t
    {
        mock_integrand_t* mock = nullptr;

        auto operator()(real_t location) const noexcept -> real_t { return mock->call(location); }
    };

    struct mock_rule_t
    {
        MOCK_METHOD(real_t, call, (real_t left, real_t right, integrand_t const& integrand), (const, noexcept));
        virtual ~mock_rule_t() = default;
    };
    StrictMock<mock_rule_t> mock_rule;

    struct rule_t
    {
        mock_rule_t* mock = nullptr;

        auto operator()(real_t left, real_t right, integrand_t const& integrand) const noexcept -> real_t
        {
            return mock->call(left, right, integrand);
        }
    };

    struct mock_cache_t
    {
        struct result_t
        {
            real_t left_bound;
            real_t base_integral;
        };

        MOCK_METHOD(result_t, interval, (real_t location), (const, noexcept));
        virtual ~mock_cache_t() = default;
    };
    StrictMock<mock_cache_t> mock_cache;

    struct cache_t
    {
        using result_t = mock_cache_t::result_t;

        mock_cache_t* mock = nullptr;

        auto interval(real_t location) const noexcept -> result_t { return mock->interval(location); }
    };

    using jet_t = jet_t<real_t>;

    using sut_t = antiderivative_t<real_t, std::reference_wrapper<integrand_t>, rule_t, cache_t>;

    integrand_t integrand{&mock_integrand};
    sut_t       sut{std::ref(integrand), rule_t{&mock_rule}, cache_t{&mock_cache}};
};

TEST_F(quadrature_antiderivative_test_t, call)
{
    constexpr auto expected_location      = 3.71;
    constexpr auto expected_left_bound    = 1.32;
    constexpr auto expected_base_integral = 5.27;
    constexpr auto expected_residual      = 4.15;
    constexpr auto expected_tangent       = 6.08;

    EXPECT_CALL(mock_cache, interval(expected_location))
        .WillOnce(Return(cache_t::result_t{expected_left_bound, expected_base_integral}));
    EXPECT_CALL(mock_rule, call(expected_left_bound, expected_location, Ref(integrand)))
        .WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integrand, call(expected_location)).WillOnce(Return(expected_tangent));
    auto const expected = jet_t{expected_base_integral + expected_residual, expected_tangent};

    auto const actual = sut(expected_location);

    EXPECT_EQ(expected, actual);
}

// ====================================================================================================================
// antiderivative_builder_t
// ====================================================================================================================

struct quadrature_antiderivative_builder_t : Test
{
    using real_t        = float_t;
    using accumulator_t = real_t;

    using move_only_id_t = std::unique_ptr<int_t>;
    using integrand_t    = move_only_id_t;
    using rule_t         = move_only_id_t;
    using cache_t        = move_only_id_t;

    struct antiderivative_t
    {
        integrand_t integrand_;
        rule_t      rule_;
        cache_t     cache_;
    };

    struct mock_cache_builder_t
    {
        MOCK_METHOD(void, append, (real_t, real_t));
        MOCK_METHOD(cache_t, finalize, ());
        virtual ~mock_cache_builder_t() = default;
    };
    StrictMock<mock_cache_builder_t> mock_cache_builder;

    struct cache_builder_t
    {
        mock_cache_builder_t* mock = nullptr;

        auto append(real_t right_bound, real_t sum) -> void { return mock->append(right_bound, sum); }
        auto finalize() && -> cache_t { return mock->finalize(); }
    };

    static constexpr auto expected_integrand_id = 5;
    static constexpr auto expected_rule_id      = 7;
    static constexpr auto expected_cache_id     = 11;

    using sut_t
        = antiderivative_builder_t<real_t, integrand_t, rule_t, cache_builder_t, accumulator_t, antiderivative_t>;
    sut_t sut{std::make_unique<int_t>(expected_integrand_id), std::make_unique<int_t>(expected_rule_id),
              cache_builder_t{&mock_cache_builder}};

    auto expect_ids(sut_t::result_t const& actual) const -> void
    {
        EXPECT_EQ(*actual.antiderivative.integrand_, expected_integrand_id);
        EXPECT_EQ(*actual.antiderivative.rule_, expected_rule_id);
        EXPECT_EQ(*actual.antiderivative.cache_, expected_cache_id);
    }

    quadrature_antiderivative_builder_t()
    {
        EXPECT_CALL(mock_cache_builder, finalize())
            .WillOnce(Return(ByMove(std::make_unique<int_t>(expected_cache_id))));
    }
};

TEST_F(quadrature_antiderivative_builder_t, append_none)
{
    auto const actual = std::move(sut).finalize();

    expect_ids(actual);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 0.0);
    EXPECT_DOUBLE_EQ(actual.max_error, 0.0);
}

TEST_F(quadrature_antiderivative_builder_t, append_one)
{
    EXPECT_CALL(mock_cache_builder, append(1.3, 5.7));
    sut.append(1.3, 5.7, 7.11);

    auto const actual = std::move(sut).finalize();

    expect_ids(actual);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 7.11);
    EXPECT_DOUBLE_EQ(actual.max_error, 7.11);
}

TEST_F(quadrature_antiderivative_builder_t, append_many)
{
    auto const seq = InSequence{};

    EXPECT_CALL(mock_cache_builder, append(1.3, 5.7));
    EXPECT_CALL(mock_cache_builder, append(13.17, 17.19));
    EXPECT_CALL(mock_cache_builder, append(23.29, 31.37));

    sut.append(1.3, 5.7, 7.11);
    sut.append(13.17, 17.19, 53.59); // max error does not come last
    sut.append(23.29, 31.37, 41.43);

    auto const actual = std::move(sut).finalize();

    expect_ids(actual);

    EXPECT_DOUBLE_EQ(actual.achieved_error, 7.11 + 53.59 + 41.43);
    EXPECT_DOUBLE_EQ(actual.max_error, 53.59);
}

} // namespace
} // namespace crv::quadrature
