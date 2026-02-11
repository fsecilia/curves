// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "stats.hpp"
#include <crv/test/test.hpp>
#include <sstream>

namespace crv {
namespace {

// ====================================================================================================================
// Histograms
// ====================================================================================================================

struct integer_histogram_test_t : Test
{
    using value_t  = int_t;
    using sut_t    = histogram_t<value_t>;
    using values_t = sut_t::values_t;

    using dump_t = std::map<value_t, int_t>;
    auto dump(sut_t const& sut) const -> dump_t
    {
        auto result = dump_t{};
        sut.visit([&](auto value, auto count) {
            result[value] = count;
            return true;
        });
        return result;
    }
};

TEST_F(integer_histogram_test_t, strips_trailing_zeros)
{
    sut_t const expected({0, 1}, {0, 0, 1});

    sut_t const actual({0, 1, 0}, {0, 0, 1, 0, 0});

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Default Constructed
// --------------------------------------------------------------------------------------------------------------------

struct integer_histogram_test_default_constructed_t : integer_histogram_test_t
{
    sut_t sut{};
};

TEST_F(integer_histogram_test_default_constructed_t, dump)
{
    EXPECT_TRUE(dump(sut).empty());
}

TEST_F(integer_histogram_test_default_constructed_t, initially_empty)
{
    EXPECT_TRUE(!sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, sample_zero)
{
    sut.sample(0);
    EXPECT_EQ((dump_t{{0, 1}}), dump(sut));
    EXPECT_EQ(1, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, single_negative)
{
    sut.sample(-3);
    EXPECT_EQ((dump_t{{-3, 1}}), dump(sut));
    EXPECT_EQ(1, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, single_positive)
{
    sut.sample(3);
    EXPECT_EQ((dump_t{{3, 1}}), dump(sut));
    EXPECT_EQ(1, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, expands_negative)
{
    sut.sample(-3);
    sut.sample(-5);
    EXPECT_EQ((dump_t{{-5, 1}, {-3, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, expands_positive)
{
    sut.sample(3);
    sut.sample(5);
    EXPECT_EQ((dump_t{{3, 1}, {5, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, does_not_contract_negative)
{
    sut.sample(-5);
    sut.sample(-3);
    EXPECT_EQ((dump_t{{-5, 1}, {-3, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, does_not_contract_positive)
{
    sut.sample(5);
    sut.sample(3);
    EXPECT_EQ((dump_t{{3, 1}, {5, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, multiple_samples_sum)
{
    sut.sample(5);
    sut.sample(-3);
    sut.sample(5);
    EXPECT_EQ((dump_t{{-3, 1}, {5, 2}}), dump(sut));
    EXPECT_EQ(3, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, equality)
{
    sut.sample(5);
    sut.sample(-3);
    sut.sample(5);
    EXPECT_EQ((sut_t{{0, 0, 0, 1}, {0, 0, 0, 0, 0, 2}}), sut);
}

TEST_F(integer_histogram_test_default_constructed_t, ostream_inserter)
{
    auto const expected = "{}";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Nontrivially Constructed
// --------------------------------------------------------------------------------------------------------------------

struct integer_histogram_test_constructed_t : integer_histogram_test_t
{
    sut_t sut{{0, 3, 7, 0, 13}, {2, 5, 0, 11, 17, 19}};
};

TEST_F(integer_histogram_test_constructed_t, dump)
{
    auto const expected = dump_t{{-4, 13}, {-2, 7}, {-1, 3}, {0, 2}, {1, 5}, {3, 11}, {4, 17}, {5, 19}};

    auto const actual = dump(sut);

    EXPECT_EQ(expected, actual);
    EXPECT_EQ(77, sut.count());
}

TEST_F(integer_histogram_test_constructed_t, copy)
{
    auto const copy = sut;
    EXPECT_EQ(sut, copy);
}

TEST_F(integer_histogram_test_constructed_t, modified_copy)
{
    auto modified_copy = sut;
    modified_copy.sample(1000);
    EXPECT_NE(sut, modified_copy);
    EXPECT_EQ(sut.count() + 1, modified_copy.count());
}

TEST_F(integer_histogram_test_constructed_t, ostream_inserter)
{
    auto const expected = "{{-4, 13}, {-2, 7}, {-1, 3}, {0, 2}, {1, 5}, {3, 11}, {4, 17}, {5, 19}}";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

} // namespace
} // namespace crv
