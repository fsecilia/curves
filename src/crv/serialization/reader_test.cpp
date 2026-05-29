// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "reader.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::serialization {
namespace {

struct serialization_reader_test_t : Test
{
    struct mock_reader_adapter_t
    {
        virtual ~mock_reader_adapter_t() = default;
        MOCK_METHOD(bool, read_bool, (std::string_view, bool&), (const));
        MOCK_METHOD(bool, read_float, (std::string_view, float_t&), (const));
        MOCK_METHOD(bool, read_string, (std::string_view, std::string&), (const));
        MOCK_METHOD(bool, get_section, (std::string_view), (const));
    };
    StrictMock<mock_reader_adapter_t> mock_reader_adapter;

    struct reader_adapter_t
    {
        mock_reader_adapter_t* mock = nullptr;

        auto read(std::string_view key, bool& dst) const -> bool { return mock->read_bool(key, dst); }
        auto read(std::string_view key, float_t& dst) const -> bool { return mock->read_float(key, dst); }
        auto read(std::string_view key, std::string& dst) const -> bool { return mock->read_string(key, dst); }

        auto get_section(std::string_view key) const -> std::optional<reader_adapter_t>
        {
            if (mock->get_section(key)) return reader_adapter_t{mock};
            return std::nullopt;
        }
    };

    struct section_t
    {
        reflection::param_t<float_t> param{"float", 2.0};

        auto reflect(this auto&& self, auto&& inspector) -> void
        {
            inspector.inspect_section(
                "section", [&](auto&& section_inspector) { self.param.reflect(section_inspector); });
        }
    };

    using sut_t = reader_t<reader_adapter_t>;
    sut_t sut{reader_adapter_t{&mock_reader_adapter}};
};

TEST_F(serialization_reader_test_t, reads_standard_types_directly)
{
    auto param = reflection::param_t<bool>{"bool", true};

    EXPECT_CALL(mock_reader_adapter, read_bool("bool", _)).WillOnce([](std::string_view, bool& dst) {
        dst = false;
        return true;
    });

    sut.inspect(param);

    EXPECT_FALSE(param.value());
}

TEST_F(serialization_reader_test_t, ignores_missing_keys)
{
    auto const original_value = 5.0;
    auto param = reflection::param_t<float_t>{"float", original_value};

    EXPECT_CALL(mock_reader_adapter, read_float("float", _)).WillOnce([](std::string_view, float_t& dst) {
        dst = 7.0;
        return false;
    });

    sut.inspect(param);

    EXPECT_EQ(original_value, param.value());
}

TEST_F(serialization_reader_test_t, inspects_nested_sections)
{
    auto section = section_t{};

    // expect section
    EXPECT_CALL(mock_reader_adapter, get_section("section")).WillOnce(Return(true));

    // expect nested parameter
    EXPECT_CALL(mock_reader_adapter, read_float("float", _)).WillOnce([](std::string_view, float_t& dst) {
        dst = 7.0;
        return true;
    });

    section.reflect(sut);

    EXPECT_DOUBLE_EQ(section.param.value(), 7.0);
}

} // namespace
} // namespace crv::serialization
