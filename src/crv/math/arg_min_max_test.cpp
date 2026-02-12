// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "arg_min_max.hpp"
#include <crv/test/test.hpp>
#include <sstream>

namespace crv {
namespace {

// ====================================================================================================================
// Arg Max
// ====================================================================================================================

struct arg_max_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;
    using sut_t   = arg_max_t<arg_t, value_t>;

    static constexpr auto old_max = 3.0;
    static constexpr auto old_arg = 5;
    static constexpr auto new_arg = 10;

    sut_t sut{.value = old_max, .arg = old_arg};
};

TEST_F(arg_max_test_t, initializes_to_min)
{
    ASSERT_EQ(min<value_t>(), sut_t{}.value);
}

TEST_F(arg_max_test_t, sample_without_new_max)
{
    auto const value = old_max - 1;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_max, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_max_test_t, first_wins)
{
    auto const value = old_max;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_max, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_max_test_t, sample_new_max)
{
    auto const new_max = old_max + 1;
    sut.sample(new_arg, new_max);

    ASSERT_EQ(new_max, sut.value);
    ASSERT_EQ(new_arg, sut.arg);
}

TEST_F(arg_max_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << old_max << "@" << old_arg;

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    ASSERT_EQ(expected.str(), actual.str());
}

// ====================================================================================================================
// Arg Min
// ====================================================================================================================

struct arg_min_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;
    using sut_t   = arg_min_t<arg_t, value_t>;

    static constexpr auto old_min = 3.0;
    static constexpr auto old_arg = 5;
    static constexpr auto new_arg = 10;

    sut_t sut{.value = old_min, .arg = old_arg};
};

TEST_F(arg_min_test_t, initializes_to_max)
{
    ASSERT_EQ(max<value_t>(), sut_t{}.value);
}

TEST_F(arg_min_test_t, sample_without_new_min)
{
    auto const value = old_min + 1;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_min, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_min_test_t, first_wins)
{
    auto const value = old_min;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_min, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_min_test_t, sample_new_min)
{
    auto const new_min = old_min - 1;
    sut.sample(new_arg, new_min);

    ASSERT_EQ(new_min, sut.value);
    ASSERT_EQ(new_arg, sut.arg);
}

TEST_F(arg_min_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << old_min << "@" << old_arg;

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    ASSERT_EQ(expected.str(), actual.str());
}

// ====================================================================================================================
// Arg Min Max
// ====================================================================================================================

struct arg_min_max_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;

    static constexpr arg_t   arg_min     = 3;
    static constexpr value_t min         = 1.1;
    static constexpr arg_t   arg_max     = 5;
    static constexpr value_t max         = 1.2;
    static constexpr arg_t   arg_max_mag = arg_max;
    static constexpr value_t max_mag     = 1.2;

    struct arg_min_t
    {
        arg_t   arg{arg_min};
        value_t value{min};

        constexpr auto sample(arg_t arg, value_t value) noexcept -> void
        {
            this->arg   = arg;
            this->value = value;
        }

        friend auto operator<<(std::ostream& out, arg_min_t const&) -> std::ostream& { return out << "arg_min"; }
    };

    struct arg_max_t
    {
        arg_t   arg{arg_min_max_test_t::arg_max};
        value_t value{arg_min_max_test_t::max};

        constexpr auto sample(arg_t arg, value_t value) noexcept -> void
        {
            this->arg   = arg;
            this->value = value;
        }

        friend auto operator<<(std::ostream& out, arg_max_t const&) -> std::ostream& { return out << "arg_max"; }
    };

    using sut_t = arg_min_max_t<arg_t, value_t, arg_min_t, arg_max_t>;
    sut_t sut{};
};

TEST_F(arg_min_max_test_t, max_mag)
{
    ASSERT_EQ(max_mag, sut.max_mag());
}

TEST_F(arg_min_max_test_t, arg_max_mag)
{
    ASSERT_EQ(arg_max_mag, sut.arg_max_mag());
}

TEST_F(arg_min_max_test_t, sample)
{
    static constexpr auto arg   = arg_t{19};
    static constexpr auto value = value_t{17.0};

    sut.sample(arg, value);

    EXPECT_EQ(arg, sut.max.arg);
    EXPECT_EQ(value, sut.max.value);
    EXPECT_EQ(arg, sut.min.arg);
    EXPECT_EQ(value, sut.min.value);
}

TEST_F(arg_min_max_test_t, ostream_inserter)
{
    auto const expected = "min = arg_min\nmax = arg_max";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    ASSERT_EQ(expected, actual.str());
}

} // namespace
} // namespace crv
