// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "writer_adapter.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace serialization::tomlpp {
namespace {

struct serialization_tomlpp_writer_adapter_test_t : Test
{
    enum class enum_t
    {
        value_0,
        value_1,
    };

    using sut_t = writer_adapter_t;
};

} // namespace
} // namespace serialization::tomlpp

namespace reflection {

template <> struct enum_t<serialization::tomlpp::serialization_tomlpp_writer_adapter_test_t::enum_t>
{
    static constexpr auto map
        = sequential_enum_name_map<serialization::tomlpp::serialization_tomlpp_writer_adapter_test_t::enum_t>(
            "value_0", "value_1");
};

} // namespace reflection

namespace serialization::tomlpp {
namespace {

TEST_F(serialization_tomlpp_writer_adapter_test_t, writes_scalar_values)
{
    auto table = toml::table{};
    auto sut = sut_t{table};

    sut.write("float", 2.5);
    sut.write("bool", true);
    sut.write("string", "value");

    // values exist, are correct types, and have correct values
    EXPECT_EQ(table["float"].value<float_t>(), 2.5);
    EXPECT_EQ(table["bool"].value<bool>(), true);
    EXPECT_EQ(table["string"].value<std::string_view>(), "value");
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, overwrites_existing_scalar_value)
{
    // pre-populate with old value
    auto table = toml::table{};
    table.insert("float", 1.0);
    auto sut = sut_t{table};

    sut.write("float", 3.0);

    EXPECT_EQ(table["float"].value<float_t>(), 3.0);
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, translates_enum_to_string_before_writing)
{
    auto table = toml::table{};
    auto sut = sut_t{table};

    sut.write("enum", enum_t::value_1);

    EXPECT_EQ(table["enum"].value<std::string_view>(), "value_1");
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, reports_error_on_invalid_enum_string)
{
    auto table = toml::table{};
    auto sut = sut_t{table};

    EXPECT_THROW(sut.write("enum", static_cast<enum_t>(-1)), parse_x);
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, creates_and_populates_section)
{
    auto table = toml::table{};
    auto sut = sut_t{table};

    auto section = sut.find_or_create_section("section");
    section.write("float", 5.0);

    // section must be a table
    ASSERT_TRUE(table["section"].is_table());

    // value is written inside the section
    auto& sync_table = *table["section"].as_table();
    EXPECT_EQ(sync_table["float"].value<float_t>(), 5.0);
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, appends_to_existing_section)
{
    // pre-populate section with old data
    auto table = toml::table{};
    auto old_section = toml::table{};
    old_section.insert("pre_existing_key", true);
    table.insert("section", std::move(old_section));
    auto sut = sut_t{table};

    auto section = sut.find_or_create_section("section");
    section.write("new_float", 2.0);

    auto& sync_table = *table["section"].as_table();

    // new value is written successfully
    EXPECT_EQ(sync_table["new_float"].value<float_t>(), 2.0);

    // old data is still intact
    EXPECT_TRUE(sync_table.contains("pre_existing_key"));
    EXPECT_EQ(sync_table["pre_existing_key"].value<bool>(), true);
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, reports_error_when_section_key_is_scalar)
{
    // pre-populate with a scalar value instead of a table
    auto table = toml::table{};
    table.insert("section", "string");
    auto sut = sut_t{table};

    // creating a section over a scalar throws
    EXPECT_THROW(static_cast<void>(sut.find_or_create_section("section")), parse_x);
}

} // namespace
} // namespace serialization::tomlpp
} // namespace crv
