// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_reporting.hpp"
#include <crv/test/test.hpp>

namespace crv::serialization::tomlpp {
namespace {

struct error_reporting_test_t : Test
{
    std::string message{"message"};
};

TEST_F(error_reporting_test_t, with_location)
{
    auto const path = std::string{"path"};
    auto const path_shared_ptr = std::make_shared<std::string>(path);

    auto const line = toml::source_index{5};
    auto const column = toml::source_index{7};

    auto const expected = "path:5:7: message";

    std::string actual;
    try
    {
        report_error(message,
            toml::source_region{.begin = {.line = line, .column = column}, .end = {}, .path = path_shared_ptr});
    }
    catch (std::exception& exception)
    {
        actual = exception.what();
    }

    EXPECT_EQ(expected, actual);
}

TEST_F(error_reporting_test_t, without_location)
{
    auto const expected = "message";

    std::string actual;
    try
    {
        report_error(message);
    }
    catch (std::exception& exception)
    {
        actual = exception.what();
    }

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::serialization::tomlpp
