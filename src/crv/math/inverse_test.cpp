// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "inverse.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <expected>
#include <functional>
#include <limits>
#include <optional>
#include <ostream>

namespace crv {
namespace {

using scalar_t = float_t;

auto const lowest = std::numeric_limits<scalar_t>::lowest();
auto const max = std::numeric_limits<scalar_t>::max();

template <typename value_t> [[nodiscard]] auto next(value_t value) noexcept -> value_t
{
    return std::nextafter(value, std::numeric_limits<value_t>::infinity());
}

template <typename value_t> [[nodiscard]] auto prev(value_t value) noexcept -> value_t
{
    return std::nextafter(value, -std::numeric_limits<value_t>::infinity());
}

using supported_float_types_t = Types<float32_t, float64_t>;

template <typename scalar_t> struct representable_order_key_test_t : Test
{};

TYPED_TEST_SUITE(representable_order_key_test_t, supported_float_types_t);

TYPED_TEST(representable_order_key_test_t, orders_representative_finite_values_monotonically)
{
    using scalar_t = TypeParam;
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    auto const values = std::array{std::numeric_limits<scalar_t>::lowest(), scalar_t{-1}, -denorm, scalar_t{0}, denorm,
        scalar_t{1}, std::numeric_limits<scalar_t>::max()};

    for (auto i = size_t{1}; i < values.size(); ++i)
    {
        EXPECT_LT(detail::inverse::to_key(values[i - 1]), detail::inverse::to_key(values[i]));
    }
}

TYPED_TEST(representable_order_key_test_t, collapses_both_zero_encodings_to_one_key)
{
    using scalar_t = TypeParam;
    EXPECT_EQ(detail::inverse::to_key(-scalar_t{0}), detail::inverse::to_key(scalar_t{0}));
}

TYPED_TEST(representable_order_key_test_t, zero_key_is_adjacent_to_both_minimum_subnormals)
{
    using scalar_t = TypeParam;
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    auto const negative_key = detail::inverse::to_key(-denorm);
    auto const zero_key = detail::inverse::to_key(scalar_t{0});
    auto const positive_key = detail::inverse::to_key(denorm);

    EXPECT_EQ(negative_key + 1, zero_key);
    EXPECT_EQ(zero_key + 1, positive_key);
}

TYPED_TEST(representable_order_key_test_t, round_trips_representative_values)
{
    using scalar_t = TypeParam;
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    auto const values = std::array{std::numeric_limits<scalar_t>::lowest(), scalar_t{-1}, -denorm, scalar_t{0}, denorm,
        scalar_t{1}, std::numeric_limits<scalar_t>::max()};

    for (auto const value : values)
    {
        EXPECT_EQ(detail::inverse::from_key<scalar_t>(detail::inverse::to_key(value)), value);
    }
}

TYPED_TEST(representable_order_key_test_t, round_trip_canonicalizes_negative_zero_to_positive_zero)
{
    using scalar_t = TypeParam;
    auto const actual = detail::inverse::from_key<scalar_t>(detail::inverse::to_key(-scalar_t{0}));
    EXPECT_EQ(actual, scalar_t{0});
    EXPECT_FALSE(std::signbit(actual));
}

TEST(bisect_first_true_test_t, returns_low_when_predicate_is_true_there)
{
    EXPECT_EQ(bisect_first_true_t{}(-4.0, 8.0, [](scalar_t input) noexcept { return input >= -4.0; }), -4.0);
}

TEST(bisect_first_true_test_t, returns_high_when_predicate_is_true_only_there)
{
    auto const high = next(scalar_t{1});
    EXPECT_EQ(bisect_first_true_t{}(1.0, high, [high](scalar_t input) noexcept { return input >= high; }), high);
}

TEST(bisect_first_true_test_t, returns_nullopt_when_predicate_is_always_false)
{
    EXPECT_FALSE(bisect_first_true_t{}(-4.0, 8.0, [](scalar_t) noexcept { return false; }));
}

TEST(bisect_first_true_test_t, returns_nullopt_for_single_false_point)
{
    EXPECT_FALSE(bisect_first_true_t{}(2.0, 2.0, [](scalar_t) noexcept { return false; }));
}

TEST(bisect_first_true_test_t, resolves_one_ulp_interval)
{
    auto const high = next(scalar_t{-2});
    EXPECT_EQ(bisect_first_true_t{}(-2.0, high, [high](scalar_t input) noexcept { return input == high; }), high);
}

TEST(bisect_first_true_test_t, searches_negative_only_interval)
{
    EXPECT_EQ(bisect_first_true_t{}(-10.0, -1.0, [](scalar_t input) noexcept { return input >= -3.0; }), -3.0);
}

TEST(bisect_first_true_test_t, searches_across_zero)
{
    EXPECT_EQ(bisect_first_true_t{}(-5.0, 5.0, [](scalar_t input) noexcept { return input >= 0.0; }), 0.0);
}

TEST(bisect_first_true_test_t, crosses_collapsed_zero_key_and_returns_canonical_positive_zero)
{
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    auto const result
        = bisect_first_true_t{}(-denorm, denorm, [](scalar_t input) noexcept { return input >= scalar_t{0}; });
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 0.0);
    EXPECT_FALSE(std::signbit(*result));
}

TEST(bisect_first_true_test_t, supports_lowest_finite_endpoint)
{
    auto const threshold = next(lowest);
    EXPECT_EQ(bisect_first_true_t{}(lowest, 0.0, [threshold](scalar_t input) noexcept { return input >= threshold; }),
        threshold);
}

TEST(bisect_first_true_test_t, supports_maximum_finite_endpoint)
{
    EXPECT_EQ(bisect_first_true_t{}(0.0, max, [](scalar_t input) noexcept { return input >= max; }), max);
}

TEST(bisect_first_true_test_t, finds_left_edge_of_long_true_plateau)
{
    auto const threshold = scalar_t{1};
    EXPECT_EQ(bisect_first_true_t{}(lowest, max, [threshold](scalar_t input) noexcept { return input >= threshold; }),
        threshold);
}

enum class probe_error_t : uint8_t
{
    failed,
};

TEST(try_bisect_first_true_test_t, propagates_error_from_low_probe)
{
    auto const result = try_bisect_first_true_t{}(-1.0, 1.0, [](scalar_t input) noexcept {
        if (input == -1.0) return std::expected<bool, probe_error_t>{std::unexpected{probe_error_t::failed}};
        return std::expected<bool, probe_error_t>{input >= 0.0};
    });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), probe_error_t::failed);
}

TEST(try_bisect_first_true_test_t, propagates_error_from_high_probe)
{
    auto const result = try_bisect_first_true_t{}(-1.0, 1.0, [](scalar_t input) noexcept {
        if (input == 1.0) return std::expected<bool, probe_error_t>{std::unexpected{probe_error_t::failed}};
        return std::expected<bool, probe_error_t>{false};
    });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), probe_error_t::failed);
}

TEST(try_bisect_first_true_test_t, propagates_error_from_interior_probe)
{
    auto const result = try_bisect_first_true_t{}(-1.0, 1.0, [](scalar_t input) noexcept {
        if (input == 0.0) return std::expected<bool, probe_error_t>{std::unexpected{probe_error_t::failed}};
        return std::expected<bool, probe_error_t>{input > 0.5};
    });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), probe_error_t::failed);
}

TEST(try_bisect_first_true_test_t, returns_low_without_probing_high)
{
    auto high_calls = int_t{};
    auto const result = try_bisect_first_true_t{}(-1.0, 1.0, [&high_calls](scalar_t input) noexcept {
        if (input == 1.0) ++high_calls;
        return std::expected<bool, probe_error_t>{true};
    });
    ASSERT_TRUE(result);
    ASSERT_TRUE(*result);
    EXPECT_EQ(**result, -1.0);
    EXPECT_EQ(high_calls, int_t{0});
}

using monotone_t = std::function<scalar_t(scalar_t)>;

struct lower_bound_param_t
{
    std::optional<scalar_t> expected;
    scalar_t low;
    scalar_t high;
    scalar_t target;
    monotone_t f;

    friend auto operator<<(std::ostream& out, lower_bound_param_t const& src) -> std::ostream&
    {
        out << "{expected = ";
        if (src.expected) out << *src.expected;
        else out << "nullopt";
        return out << ", low = " << src.low << ", high = " << src.high << ", target = " << src.target << "}";
    }
};

struct bisect_lower_bound_test_t : TestWithParam<lower_bound_param_t>
{};

TEST_P(bisect_lower_bound_test_t, retains_adapter_behavior)
{
    auto const& param = GetParam();
    EXPECT_EQ(param.expected, bisect_lower_bound_t{}(param.low, param.high, param.target, param.f));
}

auto const identity = [](scalar_t input) noexcept { return input; };
auto const x2 = [](scalar_t input) noexcept { return input * scalar_t{2}; };

lower_bound_param_t const well_formed_params[] = {
    {2.0, 0.0, 4.0, 2.0, identity},
    {0.0, -5.0, 5.0, 0.0, identity},
    {2.5, 0.0, 5.0, 5.0, x2},
};
INSTANTIATE_TEST_SUITE_P(well_formed, bisect_lower_bound_test_t, ValuesIn(well_formed_params));

lower_bound_param_t const boundary_conditions_params[] = {
    {0.0, 0.0, 4.0, prev(scalar_t{0}), identity},
    {0.0, 0.0, 4.0, 0.0, identity},
    {next(scalar_t{0}), 0.0, 4.0, next(scalar_t{0}), identity},
    {4.0, 0.0, 4.0, 4.0, identity},
    {prev(scalar_t{4}), 0.0, 4.0, prev(scalar_t{4}), identity},
    {std::nullopt, 0.0, 4.0, next(scalar_t{4}), identity},
};
INSTANTIATE_TEST_SUITE_P(boundary_conditions, bisect_lower_bound_test_t, ValuesIn(boundary_conditions_params));

lower_bound_param_t const edge_cases_params[] = {
    {next(scalar_t{0}), 0.0, 2.0, next(scalar_t{0}), identity},
    {next(scalar_t{0}), 0.0, 2.0, x2(next(scalar_t{0})), x2},

    {1.0, 1.0, next(scalar_t{1}), 1.0, identity},
    {next(scalar_t{1}), 1.0, next(scalar_t{1}), next(scalar_t{1}), identity},
    {1.0, 1.0, 1.0, 1.0, identity},

    {0.0, -0.0, 4.0, 0.0, identity},
    {-0.0, 0.0, 4.0, 0.0, identity},
    {-0.0, -0.0, -0.0, 0.0, identity},

    {0.0, 0.0, max, 0.0, identity},
    {max / 2, 0.0, max, max / 2, identity},
    {max, 0.0, max, max, identity},
};
INSTANTIATE_TEST_SUITE_P(edge_cases, bisect_lower_bound_test_t, ValuesIn(edge_cases_params));

// strictly linear, flat at y=1 for x in [1, 3), then resumes linear growth
auto const plateau = [](scalar_t input) noexcept {
    if (input < 1.0) return input;
    if (input < 3.0) return scalar_t{1};
    return input - 2.0;
};

lower_bound_param_t const plateau_params[] = {
    {0.0, 0.0, 5.0, 0.0, plateau},
    {next(scalar_t{0}), 0.0, 5.0, next(scalar_t{0}), plateau},

    {0.5, 0.0, 5.0, 0.5, plateau},
    {prev(scalar_t{1}), 0.0, 5.0, prev(scalar_t{1}), plateau},
    {1.0, 0.0, 5.0, 1.0, plateau},
    {next(scalar_t{3}), 0.0, 5.0, next(scalar_t{1}), plateau},
    {3.5, 0.0, 5.0, 1.5, plateau},

    {prev(scalar_t{5}), 0.0, 5.0, prev(prev(scalar_t{3})), plateau},
    {5.0, 0.0, 5.0, prev(scalar_t{3}), plateau},
    {5.0, 0.0, 5.0, 3.0, plateau},
    {std::nullopt, 0.0, 5.0, next(scalar_t{3}), plateau},
};
INSTANTIATE_TEST_SUITE_P(plateau, bisect_lower_bound_test_t, ValuesIn(plateau_params));

namespace constexpr_tests {

auto const x_squared = [](scalar_t input) noexcept { return input * input; };

static_assert(0.0 == bisect_lower_bound_t{}(0.0, 4.0, 0.0, x_squared));
static_assert(0.5 == bisect_lower_bound_t{}(0.1, 3.9, 0.25, x_squared));
static_assert(1.0 == bisect_lower_bound_t{}(0.2, 3.8, 1.0, x_squared));
static_assert(1.5 == bisect_lower_bound_t{}(0.3, 3.7, 2.25, x_squared));
static_assert(2.0 == bisect_lower_bound_t{}(0.4, 3.7, 4.0, x_squared));
static_assert(2.9 == bisect_lower_bound_t{}(2.0, 3.0, 8.41, x_squared));
static_assert(3.0 == bisect_lower_bound_t{}(2.1, 4.0, 9.0, x_squared));
static_assert(3.1 == bisect_lower_bound_t{}(2.2, 5.0, 9.61, x_squared));
static_assert(!bisect_lower_bound_t{}(2.2, 5.0, 30.0, x_squared).has_value());

constexpr auto first = bisect_first_true_t{}(-4.0, 4.0, [](scalar_t input) noexcept { return input >= 0.5; });
static_assert(first == 0.5);

constexpr auto zero = bisect_first_true_t{}(-1.0, 1.0, [](scalar_t input) noexcept { return input >= 0.0; });
static_assert(zero == 0.0);

} // namespace constexpr_tests

} // namespace
} // namespace crv
