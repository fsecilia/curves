// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "writer_adapter.hpp"
#include <crv/test/test.hpp>

namespace crv::serialization::tomlpp {
namespace {

struct serialization_tomlpp_writer_adapter_test_t : Test
{
    using sut_t = writer_adapter_t;
};

TEST_F(serialization_tomlpp_writer_adapter_test_t, writes_scalar_values)
{
    auto table = toml::table{};
    auto sut = sut_t{table};

    sut.write_value("float", 2.5);
    sut.write_value("bool", true);
    sut.write_value("string", "value");

    // values exist, are correct types, and have correct values
    EXPECT_EQ(table["float"].value<float_t>(), 2.5);
    EXPECT_EQ(table["bool"].value<bool>(), true);
    EXPECT_EQ(table["string"].value<std::string_view>(), "value");
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, creates_and_populates_section)
{
    auto table = toml::table{};
    auto sut = sut_t{table};

    auto section = sut.create_section("section");
    section.write_value("float", 5.0);

    // section must be a table
    ASSERT_TRUE(table["section"].is_table());

    // value is written inside the section
    auto& sync_table = *table["section"].as_table();
    EXPECT_EQ(sync_table["float"].value<float_t>(), 5.0);
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, overwrites_existing_value)
{
    // pre-populate with old value
    auto table = toml::table{};
    table.insert("float", 1.0);
    auto sut = sut_t{table};

    sut.write_value("float", 3.0);

    EXPECT_EQ(table["float"].value<float_t>(), 3.0);
}

TEST_F(serialization_tomlpp_writer_adapter_test_t, overwrites_existing_section)
{
    // pre-populate section with old data
    auto table = toml::table{};
    toml::table old_section;
    old_section.insert("garbage", true);
    table.insert("section", std::move(old_section));
    auto sut = sut_t{table};

    auto section = sut.create_section("section");
    section.write_value("float", 2.0);

    auto& sync_table = *table["section"].as_table();
    EXPECT_EQ(sync_table["float"].value<float_t>(), 2.0);
    EXPECT_FALSE(sync_table.contains("garbage")); // old data must be gone
}

} // namespace
} // namespace crv::serialization::tomlpp
