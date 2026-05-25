// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "error_reporter.hpp"
#include <crv/test/test.hpp>
#include <memory>
#include <string>

namespace crv::serialization::tomlpp {
namespace {

struct config_error_reporter_test_t : Test
{
    using src_path_t = toml::source_path_ptr;
    src_path_t const expected_source_path = std::make_shared<std::string>("source_path");
    toml::source_region const location{.begin = {}, .end = {}, .path = expected_source_path};

    using sut_t = error_reporter_t;

    sut_t sut{};
};

TEST_F(config_error_reporter_test_t, location_is_recorded)
{
    sut.location(location);

    // Location is only readable from the exception after throwing.
    auto actual_source_path = src_path_t{};
    try
    {
        sut.report_error("");
    }
    catch (toml::parse_error const& err)
    {
        actual_source_path = err.source().path;
    }

    ASSERT_EQ(expected_source_path, actual_source_path);
}

TEST_F(config_error_reporter_test_t, message)
{
    auto const expected_message = std::string{"expected_message"};

    // message is only recorded in thrown exception.
    auto actual_message = std::string{};
    try
    {
        sut.report_error(expected_message);
    }
    catch (toml::parse_error const& err)
    {
        actual_message = err.description();
    }

    ASSERT_EQ(expected_message, actual_message);
}

} // namespace
} // namespace crv::serialization::tomlpp
