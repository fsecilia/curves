// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "writer.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv {
namespace serialization {
namespace {

struct serialization_writer_test_t : Test
{
    struct mock_writer_adapter_t
    {
        virtual ~mock_writer_adapter_t() = default;
        MOCK_METHOD(void, write_bool, (std::string_view, bool const&), (const));
        MOCK_METHOD(void, write_float, (std::string_view, float_t const&), (const));
        MOCK_METHOD(void, write_string, (std::string_view, std::string_view), (const));
        MOCK_METHOD(void, create_section, (std::string_view));
    };
    StrictMock<mock_writer_adapter_t> mock_writer_adapter;

    struct writer_adapter_t
    {
        mock_writer_adapter_t* mock = nullptr;

        auto write(std::string_view key, bool const& src) const -> void { mock->write_bool(key, src); }
        auto write(std::string_view key, float_t const& src) const -> void { mock->write_float(key, src); }
        auto write(std::string_view key, std::string_view src) const -> void { mock->write_string(key, src); }

        auto create_section(std::string_view key) -> writer_adapter_t
        {
            mock->create_section(key);
            return writer_adapter_t{mock};
        }
    };

    struct section_t
    {
        reflection::param_t<float_t> param{"float", 2.0};

        auto reflect(this auto&& self, auto&& visitor) -> void
        {
            visitor.visit_section("section", [&](auto&& section_visitor) { self.param.reflect(section_visitor); });
        }
    };

    using sut_t = writer_t<writer_adapter_t>;
    sut_t sut{writer_adapter_t{&mock_writer_adapter}};
};

TEST_F(serialization_writer_test_t, writes_standard_types_directly)
{
    auto param = reflection::param_t<bool>{"bool", true};

    EXPECT_CALL(mock_writer_adapter, write_bool("bool", true));

    sut(param);
}

TEST_F(serialization_writer_test_t, visits_nested_sections)
{
    auto section = section_t{};

    // create section
    EXPECT_CALL(mock_writer_adapter, create_section("section"));

    // write to section
    EXPECT_CALL(mock_writer_adapter, write_float("float", 2.0));

    section.reflect(sut);
}

} // namespace
} // namespace serialization
} // namespace crv
