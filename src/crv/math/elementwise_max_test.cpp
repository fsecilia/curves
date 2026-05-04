// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "elementwise_max.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <compare>

namespace crv::cpo {
namespace {

struct has_friend_elementwise_max_t
{
    int_t value;

    friend constexpr auto elementwise_max(has_friend_elementwise_max_t const& lhs,
        has_friend_elementwise_max_t const& rhs) noexcept -> has_friend_elementwise_max_t
    {
        return {lhs.value > rhs.value ? lhs.value : rhs.value};
    }

    constexpr auto operator==(has_friend_elementwise_max_t const&) const noexcept -> bool = default;
};

// --------------------------------------------------------------------------------------------------------------------
// constexpr test
// --------------------------------------------------------------------------------------------------------------------

namespace constexpr_test {

constexpr auto lhs = has_friend_elementwise_max_t{5};
constexpr auto rhs = has_friend_elementwise_max_t{10};
constexpr auto result = elementwise_max(lhs, rhs);
static_assert(result.value == rhs.value);

} // namespace constexpr_test

// --------------------------------------------------------------------------------------------------------------------
// typed test
// --------------------------------------------------------------------------------------------------------------------

namespace typed_test {

struct has_friend_max_t
{
    int_t value;

    friend constexpr auto max(has_friend_max_t const& lhs, has_friend_max_t const& rhs) noexcept
        -> has_friend_max_t const&
    {
        return rhs.value < lhs.value ? lhs : rhs;
    }

    constexpr auto operator==(has_friend_max_t const&) const noexcept -> bool = default;
};

struct has_standalone_max_t
{
    int_t value;

    constexpr auto operator==(has_standalone_max_t const&) const noexcept -> bool = default;
};

constexpr auto max(has_standalone_max_t const& lhs, has_standalone_max_t const& rhs) noexcept
    -> has_standalone_max_t const&
{
    return rhs.value < lhs.value ? lhs : rhs;
}

struct has_strong_ordering_t
{
    int_t value;

    constexpr auto operator<=>(has_strong_ordering_t const&) const noexcept -> std::strong_ordering = default;
};

struct has_partial_ordering_t
{
    int_t value;

    constexpr auto operator<=>(has_partial_ordering_t const&) const noexcept -> std::partial_ordering = default;
};

template <typename value_t> struct elementwise_max_typed_test_t : Test
{};

using test_types_t = Types<int_t, uint_t, float_t, has_friend_elementwise_max_t, has_friend_max_t, has_standalone_max_t,
    has_strong_ordering_t, has_partial_ordering_t>;
TYPED_TEST_SUITE(elementwise_max_typed_test_t, test_types_t);

TYPED_TEST(elementwise_max_typed_test_t, lhs_max)
{
    auto const& lhs = TypeParam{3};
    auto const& rhs = TypeParam{2};

    auto const& result = elementwise_max(lhs, rhs);
    EXPECT_EQ(lhs, result);
    EXPECT_NE(&lhs, &result);
    EXPECT_NE(&rhs, &result);
}

TYPED_TEST(elementwise_max_typed_test_t, rhs_max)
{
    auto const& lhs = TypeParam{2};
    auto const& rhs = TypeParam{3};

    auto const& result = elementwise_max(lhs, rhs);
    EXPECT_EQ(rhs, result);
    EXPECT_NE(&lhs, &result);
    EXPECT_NE(&rhs, &result);
}

TYPED_TEST(elementwise_max_typed_test_t, equal_max)
{
    auto const& lhs = TypeParam{3};
    auto const& rhs = TypeParam{3};

    auto const& result = elementwise_max(lhs, rhs);
    EXPECT_EQ(lhs, result);
    EXPECT_EQ(rhs, result);
    EXPECT_NE(&lhs, &result);
    EXPECT_NE(&rhs, &result);
}

} // namespace typed_test

// --------------------------------------------------------------------------------------------------------------------
// parameterized test
// --------------------------------------------------------------------------------------------------------------------

namespace parameterized_test {

using scalar_t = int_t;

struct vector_t
{
    using values_t = std::array<scalar_t, 2>;
    values_t values;

    friend constexpr auto elementwise_max(vector_t const& left, vector_t const& right) noexcept -> vector_t
    {
        return {{elementwise_max(left.values[0], right.values[0]), elementwise_max(left.values[1], right.values[1])}};
    }

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{" << src.values[0] << ", " << src.values[1] << "}";
    }
};

struct param_t
{
    vector_t lhs;
    vector_t rhs;
    vector_t expected;

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        return out << "{.lhs = " << src.lhs << ", .rhs = " << src.rhs << ", .expected = " << src.expected << "}";
    }
};

struct elementwise_max_parameterized_test_t : TestWithParam<param_t>
{
    vector_t const& lhs = GetParam().lhs;
    vector_t const& rhs = GetParam().rhs;
    vector_t const& expected = GetParam().expected;
};

TEST_P(elementwise_max_parameterized_test_t, result)
{
    auto const& actual = elementwise_max(lhs, rhs);

    EXPECT_NE(&lhs, &actual);
    EXPECT_NE(&rhs, &actual);

    EXPECT_EQ(expected.values[0], actual.values[0]);
    EXPECT_EQ(expected.values[1], actual.values[1]);
}

auto const max = crv::max<scalar_t>();
param_t const params[] = {
    {{0, max}, {0, 0}, {0, max}},

    {{0, 0}, {0, 0}, {0, 0}},

    {{0, 0}, {0, max}, {0, max}},
    {{0, max - 1}, {0, max}, {0, max}},

    {{0, 0}, {max, 0}, {max, 0}},
    {{max - 1, 0}, {max, 0}, {max, 0}},

    {{0, max}, {0, 0}, {0, max}},
    {{0, max}, {0, max - 1}, {0, max}},

    {{max, 0}, {0, 0}, {max, 0}},
    {{max, 0}, {max - 1, 0}, {max, 0}},

    {{max, 0}, {0, max}, {max, max}},
    {{max, 0}, {max - 1, max}, {max, max}},

    {{max, max - 1}, {0, max}, {max, max}},
    {{max, max - 1}, {max - 1, max}, {max, max}},
};
INSTANTIATE_TEST_SUITE_P(params, elementwise_max_parameterized_test_t, ValuesIn(params));

} // namespace parameterized_test
} // namespace
} // namespace crv::cpo
