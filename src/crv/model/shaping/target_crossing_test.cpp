// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "target_crossing.hpp"
#include <crv/test/test.hpp>
#include <functional>
#include <gmock/gmock.h>
#include <limits>
#include <optional>

namespace crv::shaping {
namespace {

using scalar_t = float_t;
using comparison_t = crossing_comparison_t;
using outcome_t = crossing_result_t<scalar_t>;

// non-enum error verifies that the original payload is preserved
struct error_t
{
    int_t code;
    scalar_t input;
    auto operator==(error_t const&) const noexcept -> bool = default;
};

struct target_crossing_test_t : Test
{
    using evaluation_t = std::expected<scalar_t, error_t>;
    using predicate_result_t = std::expected<bool, error_t>;
    using predicate_t = std::function<predicate_result_t(scalar_t)>;
    using search_result_t = std::expected<std::optional<scalar_t>, error_t>;

    struct mock_curve_t
    {
        virtual ~mock_curve_t() = default;
        MOCK_METHOD(evaluation_t, try_evaluate, (scalar_t), (const, noexcept));
        MOCK_METHOD(scalar_t, trusted, (scalar_t), (const, noexcept));
        auto operator()(scalar_t input) const noexcept -> scalar_t { return trusted(input); }
    };
    StrictMock<mock_curve_t> mock_curve;

    struct mock_search_t
    {
        virtual ~mock_search_t() = default;
        MOCK_METHOD(search_result_t, search, (scalar_t, scalar_t, predicate_t const&), (const, noexcept));
    };
    StrictMock<mock_search_t> mock_search;

    struct search_t
    {
        mock_search_t const* mock;
        auto operator()(scalar_t low, scalar_t high, predicate_t const& predicate) const noexcept -> search_result_t
        {
            return mock->search(low, high, predicate);
        }
    };

    target_crossing_t<search_t> sut{{&mock_search}};
    static constexpr auto low = scalar_t{1};
    static constexpr auto high = scalar_t{9};
    static constexpr auto target = scalar_t{4};
    static constexpr auto coordinate = scalar_t{3};
    static constexpr auto error = error_t{7, coordinate};
    static constexpr auto infinity = std::numeric_limits<scalar_t>::infinity();

    auto resolve(comparison_t comparison = comparison_t::inclusive) const noexcept
    {
        return sut(low, high, target, comparison, mock_curve);
    }
};

TEST_F(target_crossing_test_t, forwards_exact_search_bounds_and_returns_absence_without_evaluation)
{
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce(Return(search_result_t{std::nullopt}));
    EXPECT_EQ(resolve(), outcome_t{crossing_absent_t{}});
}

struct predicate_case_t
{
    char const* name;
    comparison_t comparison;
    scalar_t output;
    bool expected;
};

struct target_crossing_predicate_test_t : target_crossing_test_t, WithParamInterface<predicate_case_t>
{};

TEST_P(target_crossing_predicate_test_t, compares_checked_output_without_adjusting_target)
{
    auto const& param = GetParam();
    std::optional<predicate_result_t> observed;
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce([&](scalar_t, scalar_t, predicate_t const& predicate) {
        observed = predicate(coordinate);
        return search_result_t{std::nullopt};
    });
    EXPECT_CALL(mock_curve, try_evaluate(coordinate)).WillOnce(Return(evaluation_t{param.output}));
    static_cast<void>(resolve(param.comparison));
    EXPECT_EQ(observed, std::optional{predicate_result_t{param.expected}});
}

predicate_case_t const predicate_cases[] = {
    {"inclusive_below", comparison_t::inclusive, 3.0, false},
    {"inclusive_equal", comparison_t::inclusive, 4.0, true},
    {"inclusive_above", comparison_t::inclusive, 5.0, true},
    {"inclusive_infinity", comparison_t::inclusive, target_crossing_test_t::infinity, true},
    {"strict_below", comparison_t::strict, 3.0, false},
    {"strict_equal", comparison_t::strict, 4.0, false},
    {"strict_above", comparison_t::strict, 5.0, true},
    {"strict_infinity", comparison_t::strict, target_crossing_test_t::infinity, true},
};
INSTANTIATE_TEST_SUITE_P(
    relations, target_crossing_predicate_test_t, ValuesIn(predicate_cases), test_name_generator_t<predicate_case_t>{});

TEST_F(target_crossing_test_t, forwards_evaluation_error_to_search_predicate)
{
    std::optional<predicate_result_t> observed;
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce([&](scalar_t, scalar_t, predicate_t const& predicate) {
        observed = predicate(coordinate);
        return search_result_t{std::unexpected{error}};
    });
    EXPECT_CALL(mock_curve, try_evaluate(coordinate)).WillOnce(Return(evaluation_t{std::unexpected{error}}));
    static_cast<void>(resolve());
    EXPECT_EQ(observed, std::optional{predicate_result_t{std::unexpected{error}}});
}

TEST_F(target_crossing_test_t, propagates_search_failure_without_classification_evaluation)
{
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce(Return(search_result_t{std::unexpected{error}}));
    EXPECT_EQ(resolve(), std::unexpected{error});
}

TEST_F(target_crossing_test_t, classifies_selected_finite_coordinate_using_checked_evaluation)
{
    InSequence sequence;
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce(Return(search_result_t{coordinate}));
    EXPECT_CALL(mock_curve, try_evaluate(coordinate)).WillOnce(Return(evaluation_t{target}));
    EXPECT_EQ(resolve(), outcome_t{finite_crossing_t<scalar_t>{coordinate}});
}

TEST_F(target_crossing_test_t, classifies_selected_infinite_output_as_frontier)
{
    InSequence sequence;
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce(Return(search_result_t{coordinate}));
    EXPECT_CALL(mock_curve, try_evaluate(coordinate)).WillOnce(Return(evaluation_t{infinity}));
    EXPECT_EQ(resolve(), outcome_t{crossing_frontier_t<scalar_t>{coordinate}});
}

TEST_F(target_crossing_test_t, propagates_classification_evaluation_error)
{
    InSequence sequence;
    EXPECT_CALL(mock_search, search(low, high, _)).WillOnce(Return(search_result_t{coordinate}));
    EXPECT_CALL(mock_curve, try_evaluate(coordinate)).WillOnce(Return(evaluation_t{std::unexpected{error}}));
    EXPECT_EQ(resolve(), std::unexpected{error});
}

} // namespace
} // namespace crv::shaping
