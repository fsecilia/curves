// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "static_vector.hpp"
#include <crv/test/test.hpp>
#include <inplace_vector>
#include <numeric>
#include <ranges>

namespace crv {
namespace {

// ====================================================================================================================
// trivially copyable value test
// ====================================================================================================================

namespace trivially_copyable_tests {

using value_t = int_t;

// --------------------------------------------------------------------------------------------------------------------
// 0 size
// --------------------------------------------------------------------------------------------------------------------

namespace zero_size {

using sut_t = static_vector_t<value_t, 0>;
constexpr auto const empty_initializer_list = std::initializer_list<value_t>{};

//
// construction
//

// empty iterator pair
constexpr auto construct_from_iterator_pair() noexcept -> sut_t
{
    return static_vector_t<value_t, 0>{std::begin(empty_initializer_list), std::end(empty_initializer_list)};
}
static_assert(construct_from_iterator_pair().empty());

// empty initializer list
constexpr auto construct_from_initializer_list() noexcept -> sut_t
{
    return static_vector_t<value_t, 0>{empty_initializer_list};
}
static_assert(construct_from_initializer_list().empty());

// empty range
constexpr auto construct_from_range() noexcept -> sut_t
{
    return static_vector_t<value_t, 0>{std::from_range, empty_initializer_list};
}
static_assert(construct_from_range().empty());

#if !defined NDEBUG && defined CRV_ENABLE_DEATH_TESTS

TEST(static_vector_trivialy_copyable_zero_size, ctor_larger_iterator_pair_asserts)
{
    auto const larger_initializer_list = std::initializer_list{0};
    EXPECT_DEBUG_DEATH(static_cast<void>(static_vector_t<value_t, 0>{
                           std::begin(larger_initializer_list), std::end(larger_initializer_list)}),
        "range exceeds capacity");
}

TEST(static_vector_trivialy_copyable_zero_size, ctor_larger_initializer_list_asserts)
{
    // nonempty initializer_list asserts
    EXPECT_DEBUG_DEATH(
        static_cast<void>(static_vector_t<value_t, 0>{std::initializer_list<value_t>{0}}), "range exceeds capacity");
}

TEST(static_vector_trivialy_copyable_zero_size, ctor_larger_range_asserts)
{
    // empty initializer_list is safe
    [[maybe_unused]] auto const safe = static_vector_t<value_t, 0>{std::from_range, std::initializer_list<value_t>{}};

    // nonempty initializer_list asserts
    EXPECT_DEBUG_DEATH(
        static_cast<void>(static_vector_t<value_t, 0>{std::from_range, std::initializer_list<value_t>{0}}),
        "range exceeds capacity");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS

// copy ctor
constexpr auto construct_from_copy() noexcept -> sut_t
{
    auto const src = sut_t{};
    return sut_t{src};
}
static_assert(construct_from_copy().empty());

// move ctor
static_assert(sut_t{sut_t{}}.empty());

//
// assignment
//

// copy assign
constexpr auto construct_from_copy_assign() noexcept -> sut_t
{
    auto const src = sut_t{};
    auto dst = sut_t{};
    return dst = src;
}
static_assert(construct_from_copy_assign().empty());

// move assign
constexpr auto construct_from_move_assign() noexcept -> sut_t
{
    auto dst = sut_t{};
    return dst = sut_t{};
}
static_assert(construct_from_move_assign().empty());

//
// comparison
//

static_assert(std::is_eq(sut_t{} <=> sut_t{}));
static_assert(sut_t{} == sut_t{});

//
// iterators
//

} // namespace zero_size
} // namespace trivially_copyable_tests

struct static_vector_test_t : Test
{
    template <std::ranges::range range_t> auto sum_range(range_t const& r)
    {
        using T = std::ranges::range_value_t<range_t>;
        return std::accumulate(r.begin(), r.end(), T{0});
    }
};

TEST_F(static_vector_test_t, initializer_list_construction)
{
    auto const vec = static_vector_t<float, 5>{1.0f, 2.0f, 3.0f};

    EXPECT_EQ(vec.size(), 3);
    EXPECT_FALSE(vec.empty());
    EXPECT_FLOAT_EQ(vec[0], 1.0f);
    EXPECT_FLOAT_EQ(vec[2], 3.0f);
}

TEST_F(static_vector_test_t, push_back)
{
    static_vector_t<int, 3> vec;
    EXPECT_TRUE(vec.empty());

    vec.push_back(10);
    vec.push_back(20);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[1], 20);
}

TEST_F(static_vector_test_t, is_range)
{
    auto const vec = static_vector_t<double, 4>{0.5, 0.5, 1.0};

    auto const sum = sum_range(vec);

    EXPECT_DOUBLE_EQ(sum, 2.0);
}

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS

namespace death_tests {

TEST_F(static_vector_test_t, DeathOnOverflow)
{
    static_vector_t<int, 1> vec;
    vec.push_back(1);
    EXPECT_DEATH(vec.push_back(2), "overflow");
}

} // namespace death_tests

#endif

} // namespace
} // namespace crv
