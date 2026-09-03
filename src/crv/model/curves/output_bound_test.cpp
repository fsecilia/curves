// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_bound.hpp"
#include <crv/test/test.hpp>
#include <concepts>
#include <expected>
#include <gmock/gmock.h>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace crv::model::curves {
namespace {

struct curve_output_bound_test_base_t : Test
{
    using scalar_t = float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;
    using checked_result_t = std::expected<scalar_t, shaping::curve_evaluation_error_t>;
    using bound_result_t = curve_output_bound_result_t<scalar_t>;

    struct mock_curve_t
    {
        virtual ~mock_curve_t() = default;

        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(input_domain_t, input_domain, (), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
        MOCK_METHOD(checked_result_t, try_evaluate, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_curve_t> mock_curve;

    struct curve_t
    {
        using scalar_t = curve_output_bound_test_base_t::scalar_t;

        mock_curve_t* mock;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return mock->input_domain();
        }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return mock->critical_points(); }
        [[nodiscard]] auto try_evaluate(scalar_t input) const noexcept -> checked_result_t
        {
            return mock->try_evaluate(input);
        }
    };

    static_assert(is_curve<curve_t, scalar_t>);

    curve_t curve{&mock_curve};
};

struct curve_output_bound_search_test_t : curve_output_bound_test_base_t
{
    using first_true_result_t = std::expected<std::optional<scalar_t>, shaping::curve_evaluation_error_t>;

    struct mock_first_true_search_t
    {
        virtual ~mock_first_true_search_t() = default;
        MOCK_METHOD(void, call, (scalar_t low, scalar_t high), (const, noexcept));
    };
    StrictMock<mock_first_true_search_t> mock_first_true_search;

    struct first_true_search_state_t
    {
        std::optional<scalar_t> probe;
        first_true_result_t result{std::optional<scalar_t>{}};
    };

    struct first_true_search_t
    {
        mock_first_true_search_t* mock;
        first_true_search_state_t* state;

        template <typename predicate_t>
        [[nodiscard]] auto operator()(scalar_t low, scalar_t high, predicate_t const& predicate) const noexcept
            -> first_true_result_t
        {
            mock->call(low, high);
            if (!state->probe) return state->result;

            auto const predicate_result = predicate(*state->probe);
            if (!predicate_result) return std::unexpected{predicate_result.error()};
            if (*predicate_result) return std::optional<scalar_t>{*state->probe};
            return std::optional<scalar_t>{};
        }
    };

    struct mock_relation_t
    {
        virtual ~mock_relation_t() = default;
        MOCK_METHOD(bool, call, (scalar_t output, scalar_t target), (const, noexcept));
    };
    StrictMock<mock_relation_t> mock_relation;

    struct relation_t
    {
        mock_relation_t* mock;

        [[nodiscard]] auto operator()(scalar_t output, scalar_t target) const noexcept -> bool
        {
            return mock->call(output, target);
        }
    };

    first_true_search_state_t first_true_search_state;

    curve_output_bound_search_t<first_true_search_t> sut{{&mock_first_true_search, &first_true_search_state}};
    relation_t relation{&mock_relation};
};

TEST_F(curve_output_bound_search_test_t, empty_search_domain_returns_absence_without_consulting_dependencies)
{
    EXPECT_EQ(sut(curve, input_domain_t{}, scalar_t{5}, relation), bound_result_t{std::optional<scalar_t>{}});
}

TEST_F(curve_output_bound_search_test_t, forwards_supplied_interval_to_first_true_search)
{
    auto const search_domain = input_domain_t{2, 7};
    EXPECT_CALL(mock_curve, input_domain()).WillOnce(Return(input_domain_t::full()));
    EXPECT_CALL(mock_first_true_search, call(scalar_t{2}, scalar_t{7}));
    EXPECT_EQ(sut(curve, search_domain, scalar_t{5}, relation), bound_result_t{std::optional<scalar_t>{}});
}

TEST_F(curve_output_bound_search_test_t, first_true_predicate_uses_checked_evaluation_and_relation)
{
    first_true_search_state.probe = scalar_t{3};
    EXPECT_CALL(mock_curve, input_domain()).WillOnce(Return(input_domain_t::full()));
    EXPECT_CALL(mock_first_true_search, call(scalar_t{2}, scalar_t{7}));
    EXPECT_CALL(mock_curve, try_evaluate(scalar_t{3})).WillOnce(Return(checked_result_t{scalar_t{5}}));
    EXPECT_CALL(mock_relation, call(scalar_t{5}, scalar_t{7})).WillOnce(Return(false));
    EXPECT_EQ(sut(curve, input_domain_t{2, 7}, scalar_t{7}, relation), bound_result_t{std::optional<scalar_t>{}});
}

struct curve_output_bound_search_evaluation_error_test_t : curve_output_bound_search_test_t,
                                                           WithParamInterface<shaping::curve_evaluation_error_t>
{};

TEST_P(curve_output_bound_search_evaluation_error_test_t, propagates_checked_evaluation_error_from_search_predicate)
{
    auto const error = GetParam();
    first_true_search_state.probe = scalar_t{3};
    EXPECT_CALL(mock_curve, input_domain()).WillOnce(Return(input_domain_t::full()));
    EXPECT_CALL(mock_first_true_search, call(scalar_t{2}, scalar_t{7}));
    EXPECT_CALL(mock_curve, try_evaluate(scalar_t{3})).WillOnce(Return(std::unexpected{error}));

    auto const expected = bound_result_t{
        std::unexpected{curve_output_bound_errors_t{std::in_place_type<shaping::curve_evaluation_error_t>, error}}};
    EXPECT_EQ(sut(curve, input_domain_t{2, 7}, scalar_t{5}, relation), expected);
}

INSTANTIATE_TEST_SUITE_P(all_checked_errors, curve_output_bound_search_evaluation_error_test_t,
    Values(shaping::curve_evaluation_error_t::negative_finite_result,
        shaping::curve_evaluation_error_t::negative_infinity, shaping::curve_evaluation_error_t::nan));

TEST_F(curve_output_bound_search_test_t, finite_first_true_is_rechecked_and_returned)
{
    first_true_search_state.result = std::optional<scalar_t>{3};
    EXPECT_CALL(mock_curve, input_domain()).WillOnce(Return(input_domain_t::full()));
    EXPECT_CALL(mock_first_true_search, call(scalar_t{2}, scalar_t{7}));
    EXPECT_CALL(mock_curve, try_evaluate(scalar_t{3})).WillOnce(Return(checked_result_t{scalar_t{5}}));
    EXPECT_CALL(mock_relation, call(scalar_t{5}, scalar_t{4})).WillOnce(Return(true));
    EXPECT_EQ(sut(curve, input_domain_t{2, 7}, scalar_t{4}, relation), bound_result_t{std::optional<scalar_t>{3}});
}

TEST_F(curve_output_bound_search_test_t, checked_error_while_rechecking_first_true_is_propagated)
{
    auto const error = shaping::curve_evaluation_error_t::nan;
    first_true_search_state.result = std::optional<scalar_t>{3};
    EXPECT_CALL(mock_curve, input_domain()).WillOnce(Return(input_domain_t::full()));
    EXPECT_CALL(mock_first_true_search, call(scalar_t{2}, scalar_t{7}));
    EXPECT_CALL(mock_curve, try_evaluate(scalar_t{3})).WillOnce(Return(std::unexpected{error}));

    auto const expected = bound_result_t{
        std::unexpected{curve_output_bound_errors_t{std::in_place_type<shaping::curve_evaluation_error_t>, error}}};
    EXPECT_EQ(sut(curve, input_domain_t{2, 7}, scalar_t{4}, relation), expected);
}

TEST_F(curve_output_bound_search_test_t, positive_infinity_at_first_true_returns_bound_resolution_error)
{
    first_true_search_state.result = std::optional<scalar_t>{3};
    EXPECT_CALL(mock_curve, input_domain()).WillOnce(Return(input_domain_t::full()));
    EXPECT_CALL(mock_first_true_search, call(scalar_t{2}, scalar_t{7}));
    EXPECT_CALL(mock_curve, try_evaluate(scalar_t{3}))
        .WillOnce(Return(checked_result_t{std::numeric_limits<scalar_t>::infinity()}));

    auto const expected = bound_result_t{std::unexpected{curve_output_bound_errors_t{
        std::in_place_type<curve_output_bound_error_t>, curve_output_bound_error_t::frontier_preceded_bound}}};
    EXPECT_EQ(sut(curve, input_domain_t{2, 7}, scalar_t{4}, relation), expected);
}

struct curve_output_bound_search_contract_test_t : Test
{
    using scalar_t = float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;
    using checked_result_t = std::expected<scalar_t, shaping::curve_evaluation_error_t>;

    struct curve_t
    {
        using scalar_t = curve_output_bound_search_contract_test_t::scalar_t;

        input_domain_t domain{0, 2};
        scalar_t output{1};

        [[nodiscard]] constexpr auto operator()(scalar_t) const noexcept -> scalar_t { return output; }
        [[nodiscard]] constexpr auto input_domain() const noexcept -> input_domain_t { return domain; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
        [[nodiscard]] constexpr auto try_evaluate(scalar_t) const noexcept -> checked_result_t { return output; }
    };

    struct first_true_search_t
    {
        std::optional<scalar_t> result;

        template <typename predicate_t>
        [[nodiscard]] constexpr auto operator()(scalar_t, scalar_t, predicate_t const&) const noexcept
            -> std::expected<std::optional<scalar_t>, shaping::curve_evaluation_error_t>
        {
            return result;
        }
    };

    struct relation_t
    {
        bool result;

        [[nodiscard]] constexpr auto operator()(scalar_t, scalar_t) const noexcept -> bool { return result; }
    };
};

TEST_F(curve_output_bound_search_contract_test_t, rejects_nonfinite_target)
{
    auto const sut = curve_output_bound_search_t<first_true_search_t>{{}};
    auto const target = std::numeric_limits<scalar_t>::infinity();
    EXPECT_DEATH(
        static_cast<void>(sut(curve_t{}, input_domain_t{}, target, relation_t{true})), "target must be finite");
}

TEST_F(curve_output_bound_search_contract_test_t, rejects_search_interval_outside_curve_domain)
{
    auto const sut = curve_output_bound_search_t<first_true_search_t>{{}};
    EXPECT_DEATH(static_cast<void>(sut(curve_t{}, input_domain_t{-1, 1}, scalar_t{1}, relation_t{true})),
        "outside curve input domain");
}

TEST_F(curve_output_bound_search_contract_test_t, rejects_finite_first_true_that_does_not_satisfy_relation)
{
    auto const sut = curve_output_bound_search_t<first_true_search_t>{{scalar_t{1}}};
    EXPECT_DEATH(static_cast<void>(sut(curve_t{}, input_domain_t{0, 2}, scalar_t{1}, relation_t{false})),
        "does not satisfy relation");
}

struct curve_output_bound_relation_test_base_t : Test
{
    using scalar_t = float_t;
    static constexpr auto target = scalar_t{5};
};

struct curve_output_lower_bound_relation_test_t : curve_output_bound_relation_test_base_t
{
    curve_output_lower_bound_relation_t sut;
};

TEST_F(curve_output_lower_bound_relation_test_t, output_below_target_does_not_satisfy_relation)
{
    EXPECT_FALSE(sut(scalar_t{4}, target));
}

TEST_F(curve_output_lower_bound_relation_test_t, output_equal_to_target_satisfies_relation)
{
    EXPECT_TRUE(sut(target, target));
}

TEST_F(curve_output_lower_bound_relation_test_t, output_above_target_satisfies_relation)
{
    EXPECT_TRUE(sut(scalar_t{6}, target));
}

struct curve_output_upper_bound_relation_test_t : curve_output_bound_relation_test_base_t
{
    curve_output_upper_bound_relation_t sut;
};

TEST_F(curve_output_upper_bound_relation_test_t, output_below_target_does_not_satisfy_relation)
{
    EXPECT_FALSE(sut(scalar_t{4}, target));
}

TEST_F(curve_output_upper_bound_relation_test_t, output_equal_to_target_does_not_satisfy_relation)
{
    EXPECT_FALSE(sut(target, target));
}

TEST_F(curve_output_upper_bound_relation_test_t, output_above_target_satisfies_relation)
{
    EXPECT_TRUE(sut(scalar_t{6}, target));
}

struct curve_output_bound_composition_test_t : Test
{
    using scalar_t = float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;
    using result_t = curve_output_bound_result_t<scalar_t>;

    struct curve_t
    {
        using scalar_t = curve_output_bound_composition_test_t::scalar_t;
        scalar_t marker;
    };

    struct relation_t
    {
        scalar_t marker;
    };

    struct mock_search_t
    {
        virtual ~mock_search_t() = default;
        MOCK_METHOD(result_t, call,
            (scalar_t curve_marker, input_domain_t search_domain, scalar_t target, scalar_t relation_marker),
            (const, noexcept));
    };
    StrictMock<mock_search_t> mock_search;

    struct search_t
    {
        mock_search_t* mock;

        [[nodiscard]] auto operator()(curve_t const& curve, input_domain_t search_domain, scalar_t target,
            relation_t const& relation) const noexcept -> result_t
        {
            return mock->call(curve.marker, search_domain, target, relation.marker);
        }
    };

    using sut_t = curve_output_bound_t<search_t, relation_t>;
    static_assert(std::same_as<sut_t::error_t, curve_output_bound_errors_t>);
    static_assert(std::same_as<sut_t::result_t<scalar_t>, curve_output_bound_result_t<scalar_t>>);

    static constexpr auto curve_marker = scalar_t{3};
    static constexpr auto relation_marker = scalar_t{7};
    sut_t sut{{&mock_search}, {relation_marker}};
};

TEST_F(curve_output_bound_composition_test_t, forwards_curve_interval_target_and_relation_to_search)
{
    auto const search_domain = input_domain_t{2, 5};
    auto const target = scalar_t{11};
    auto const expected = result_t{std::optional<scalar_t>{13}};
    EXPECT_CALL(mock_search, call(curve_marker, search_domain, target, relation_marker)).WillOnce(Return(expected));
    EXPECT_EQ(sut(curve_t{curve_marker}, search_domain, target), expected);
}

static_assert(std::same_as<curve_output_lower_bound_t,
    curve_output_bound_t<curve_output_bound_search_t<try_bisect_first_true_t>, curve_output_lower_bound_relation_t>>);
static_assert(std::same_as<curve_output_upper_bound_t,
    curve_output_bound_t<curve_output_bound_search_t<try_bisect_first_true_t>, curve_output_upper_bound_relation_t>>);

} // namespace
} // namespace crv::model::curves
