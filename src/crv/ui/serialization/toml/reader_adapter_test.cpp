// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "reader_adapter.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace serialization::tomlpp {
namespace {

struct serialization_tomlpp_reader_adapter_test_t : Test
{
    enum class enum_t
    {
        value_0,
        value_1,
    };

    using sut_t = reader_adapter_t;

    static auto parse(std::string_view toml_str) -> toml::table { return toml::parse(toml_str); }
};

} // namespace
} // namespace serialization::tomlpp

namespace reflection {

template <> struct enum_t<serialization::tomlpp::serialization_tomlpp_reader_adapter_test_t::enum_t>
{
    static constexpr auto map
        = sequential_enum_name_map<serialization::tomlpp::serialization_tomlpp_reader_adapter_test_t::enum_t>(
            "value_0", "value_1");
};

} // namespace reflection

namespace serialization::tomlpp {
namespace {

TEST_F(serialization_tomlpp_reader_adapter_test_t, reads_scalar_value)
{
    auto const table = parse(R"( key = 2.5 )");
    auto sut = sut_t{table};

    auto value = 1.0;
    auto const actual = sut.read("key", value);

    EXPECT_TRUE(actual);

    EXPECT_DOUBLE_EQ(value, 2.5);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, ignores_missing_scalar_value_without_error)
{
    auto const table = parse(R"( key = 2.5 )");
    auto sut = sut_t{table};

    auto const original_value = 1.0;
    auto value = original_value;
    auto const actual = sut.read("other_key", value);

    EXPECT_FALSE(actual);

    EXPECT_DOUBLE_EQ(original_value, value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, throws_on_scalar_type_mismatch)
{
    auto const table = parse(R"( key = "value" )");
    auto sut = sut_t{table};

    auto const original_value = 1.0;
    auto value = original_value;

    EXPECT_THROW(sut.read("key", value), parse_x);

    // value is unchanged
    EXPECT_DOUBLE_EQ(original_value, value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, translates_enum_from_string)
{
    auto const table = parse(R"( enum = "value_0" )");
    auto sut = sut_t{table};

    auto const initial_value = enum_t::value_1;
    auto const expected_value = enum_t::value_0;

    enum_t value = initial_value;
    EXPECT_TRUE(sut.read("enum", value));

    EXPECT_EQ(value, expected_value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, ignores_missing_enum_without_error)
{
    auto const table = parse(R"( key = "value" )");
    auto sut = sut_t{table};

    auto const expected_value = enum_t::value_1;

    enum_t value = expected_value;
    EXPECT_FALSE(sut.read("enum", value));

    // value is unchanged
    EXPECT_EQ(value, expected_value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, throws_on_enum_type_mismatch)
{
    auto const table = parse(R"( enum = 5.0 )");
    auto sut = sut_t{table};
    auto const expected_value = enum_t::value_1;

    enum_t value = expected_value;
    EXPECT_THROW(sut.read("enum", value), parse_x);

    // value is unchanged
    EXPECT_EQ(value, expected_value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, throws_on_invalid_enum_value)
{
    auto const table = parse(R"( enum = "value_minus_1" )");
    auto sut = sut_t{table};
    auto const expected_value = enum_t::value_1;

    enum_t value = expected_value;
    EXPECT_THROW(sut.read("enum", value), parse_x);

    // value is unchanged
    EXPECT_EQ(value, expected_value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, opens_valid_section)
{
    auto const table = parse(R"(
        [section]
        key = 5.0
    )");
    auto sut = sut_t{table};

    // get nested section
    auto section = sut.get_section("section");
    EXPECT_TRUE(section.has_value());

    // read from nested section
    auto value = 0.0;

    auto const actual = section->read("key", value);

    EXPECT_TRUE(actual);

    EXPECT_DOUBLE_EQ(value, 5.0);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, throws_when_section_is_scalar)
{
    auto const table = parse(R"( key = 5.0 )"); // value instead of section
    auto sut = sut_t{table};

    EXPECT_THROW(static_cast<void>(sut.get_section("key")), parse_x);
}

} // namespace
} // namespace serialization::tomlpp
} // namespace crv
