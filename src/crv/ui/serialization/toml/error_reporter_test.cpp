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
    toml::source_region const location{
        .begin = {.line = 3, .column = 5}, .end = {.line = 7, .column = 11}, .path = expected_source_path};

    using sut_t = error_reporter_t;

    sut_t sut{};
};

TEST_F(config_error_reporter_test_t, message)
{
    sut.location(location);
    auto const thrown_message = std::string{"thrown_message"};
    auto const expected_message = std::string{"source_path:3:5: thrown_message"};

    auto actual_message = std::string{};
    try
    {
        sut.report_error(thrown_message);
    }
    catch (std::runtime_error const& err)
    {
        actual_message = err.what();
    }

    ASSERT_EQ(expected_message, actual_message);
}

} // namespace
} // namespace crv::serialization::tomlpp
