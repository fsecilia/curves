// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "reader_adapter.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::serialization::tomlpp {
namespace {

struct serialization_tomlpp_reader_adapter_test_t : Test
{
    struct mock_error_reporter_t
    {
        virtual ~mock_error_reporter_t() = default;

        MOCK_METHOD(void, location, (toml::source_region const&), (const));
        MOCK_METHOD(void, report_error, (std::string_view), (const));
    };
    StrictMock<mock_error_reporter_t> mock_error_reporter;

    using sut_t = reader_adapter_t<mock_error_reporter_t>;

    static auto parse(std::string_view toml_str) -> toml::table { return toml::parse(toml_str); }
};

TEST_F(serialization_tomlpp_reader_adapter_test_t, reads_valid_value)
{
    auto const table = parse(R"( key = 2.5 )");
    auto sut = sut_t{table, mock_error_reporter};

    EXPECT_CALL(mock_error_reporter, location(_));

    auto value = 1.0;
    auto const actual = sut.read("key", value);

    EXPECT_TRUE(actual);

    EXPECT_DOUBLE_EQ(value, 2.5);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, ignores_missing_value_without_error)
{
    auto const table = parse(R"( key = 2.5 )");
    auto sut = sut_t{table, mock_error_reporter};

    auto const original_value = 1.0;
    auto value = original_value;
    auto const actual = sut.read("other_key", value);

    EXPECT_FALSE(actual);

    EXPECT_DOUBLE_EQ(original_value, value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, reports_error_on_type_mismatch)
{
    auto const table = parse(R"( key = "value" )");
    auto sut = sut_t{table, mock_error_reporter};

    EXPECT_CALL(mock_error_reporter, location(_));
    EXPECT_CALL(mock_error_reporter, report_error(HasSubstr("type mismatch")));

    auto const original_value = 1.0;
    auto value = original_value;
    auto const actual = sut.read("key", value);

    EXPECT_FALSE(actual);

    // value is unchanged
    EXPECT_DOUBLE_EQ(original_value, value);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, opens_valid_section)
{
    auto const table = parse(R"(
        [section]
        key = 5.0
    )");
    auto sut = sut_t{table, mock_error_reporter};

    // get nested section
    EXPECT_CALL(mock_error_reporter, location(_));
    auto section = sut.get_section("section");
    EXPECT_TRUE(section.has_value());

    // read from nested section
    EXPECT_CALL(mock_error_reporter, location(_));
    auto value = 0.0;

    auto const actual = section->read("key", value);

    EXPECT_TRUE(actual);

    EXPECT_DOUBLE_EQ(value, 5.0);
}

TEST_F(serialization_tomlpp_reader_adapter_test_t, reports_error_when_section_is_scalar)
{
    auto const table = parse(R"( key = 5.0 )"); // value instead of section
    auto sut = sut_t{table, mock_error_reporter};

    EXPECT_CALL(mock_error_reporter, location(_));
    EXPECT_CALL(mock_error_reporter, report_error(HasSubstr("expected table/section")));

    auto section = sut.get_section("key");
    EXPECT_FALSE(section.has_value());
}

} // namespace
} // namespace crv::serialization::tomlpp
