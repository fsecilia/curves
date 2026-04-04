// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "priority_queue.hpp"
#include <crv/test/test.hpp>
#include <memory>
#include <string>
#include <vector>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// Standard Integer Queue
// --------------------------------------------------------------------------------------------------------------------

struct priority_queue_test_int_t : Test
{
    using sut_t = priority_queue_t<std::vector<int_t>>;
};

TEST_F(priority_queue_test_int_t, constructor_heapifies)
{
    auto sut = sut_t{{3, 1, 4, 1, 5, 9}};

    EXPECT_EQ(9, sut.top());
    sut.pop();
    EXPECT_EQ(5, sut.top());
}

TEST_F(priority_queue_test_int_t, push_and_pop_maintains_max_heap)
{
    auto sut = sut_t{};

    sut.push(10);
    sut.push(30);
    sut.push(20);
    sut.push(5);

    EXPECT_EQ(4, sut.size());

    EXPECT_EQ(30, sut.top());

    sut.pop();
    EXPECT_EQ(20, sut.top());

    sut.pop();
    EXPECT_EQ(10, sut.top());

    sut.pop();
    EXPECT_EQ(5, sut.top());
}

TEST_F(priority_queue_test_int_t, clear_empties_but_keeps_capacity)
{
    auto sut = sut_t{};
    sut.reserve(100);

    sut.push(1);
    sut.push(2);
    sut.push(3);

    EXPECT_FALSE(sut.empty());
    EXPECT_EQ(3, sut.size());
    EXPECT_GE(sut.capacity(), 100); // capacity should be at least what was reserved

    sut.clear();

    EXPECT_TRUE(sut.empty());
    EXPECT_EQ(0, sut.size());
    EXPECT_GE(sut.capacity(), 100); // capacity should survive the clear
}

TEST_F(priority_queue_test_int_t, duplicate_priorities_are_preserved)
{
    auto sut = sut_t{{5, 8, 4, 8, 9}};

    EXPECT_EQ(9, sut.top());
    sut.pop();
    EXPECT_EQ(8, sut.top());
    sut.pop();
    EXPECT_EQ(8, sut.top());
    sut.pop();
    EXPECT_EQ(5, sut.top());
}

TEST_F(priority_queue_test_int_t, pop_until_empty)
{
    auto sut = sut_t{{10, 20, 30}};

    EXPECT_FALSE(sut.empty());
    EXPECT_EQ(sut.size(), 3);

    sut.pop();
    sut.pop();
    sut.pop();

    EXPECT_TRUE(sut.empty());
    EXPECT_EQ(sut.size(), 0);
}

TEST_F(priority_queue_test_int_t, const_empty_member_functions)
{
    auto const sut = sut_t{};

    EXPECT_TRUE(sut.empty());
    EXPECT_EQ(0, sut.size());
    EXPECT_GE(sut.capacity(), 0);
}

TEST_F(priority_queue_test_int_t, const_nonempty_member_functions)
{
    auto const sut = sut_t{{1}};

    EXPECT_FALSE(sut.empty());
    EXPECT_EQ(1, sut.size());
    EXPECT_GE(sut.capacity(), 1);
    EXPECT_EQ(1, sut.top());
}

// --------------------------------------------------------------------------------------------------------------------
// Specific Value Types
// --------------------------------------------------------------------------------------------------------------------

struct priority_queue_test_t : Test
{};

TEST_F(priority_queue_test_t, emplace_constructs_in_place)
{
    using sut_t = priority_queue_t<std::vector<std::string>>;
    auto sut    = sut_t{};

    sut.push("apple");
    sut.emplace(5, 'z'); // variadic forwarding constructs std::string(5, 'z') -> "zzzzz"
    sut.push("banana");

    EXPECT_EQ("zzzzz", sut.top());
}

TEST_F(priority_queue_test_t, custom_comparator)
{
    // min-heap using std::greater
    using sut_t = priority_queue_t<std::vector<int_t>, std::greater<>>;
    auto sut    = sut_t{};

    sut.push(10);
    sut.push(2);
    sut.push(8);

    EXPECT_EQ(2, sut.top()); // smallest element should start on top

    sut.pop();
    EXPECT_EQ(8, sut.top()); // next smallest is top after pop
}

TEST_F(priority_queue_test_t, heapifying_constructor_uses_comparator_and_projection)
{
    struct item_t
    {
        int_t priority;
    };
    auto items = std::vector<item_t>{{10}, {2}, {8}};

    constexpr auto proj = [](item_t const& item) { return item.priority; };

    // min-heap using the 3-argument constructor
    using sut_t = priority_queue_t<std::vector<item_t>, std::greater<>, decltype(proj)>;
    auto sut    = sut_t{std::move(items), std::greater<>{}, proj};

    EXPECT_EQ(sut.top().priority, 2);
    sut.pop();
    EXPECT_EQ(sut.top().priority, 8);
}

TEST_F(priority_queue_test_t, push_moves_with_projection)
{
    // projection for unique ptr; takes pointer, returns dereferenced value
    constexpr auto deref_proj = [](auto const& ptr) -> auto const& { return *ptr; };

    using sut_t = priority_queue_t<std::vector<std::unique_ptr<int_t>>, std::less<>, decltype(deref_proj)>;
    auto sut    = sut_t{};

    auto       expected_top      = std::make_unique<int_t>(5296);
    auto const expected_top_data = expected_top.get();

    sut.push(std::make_unique<int_t>(3127));
    sut.push(std::move(expected_top));

    EXPECT_EQ(expected_top_data, sut.top().get());
}

} // namespace
} // namespace crv
