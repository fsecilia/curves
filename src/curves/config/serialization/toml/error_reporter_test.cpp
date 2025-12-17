// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "error_reporter.hpp"
#include <curves/testing/test.hpp>
#include <memory>
#include <string>

namespace curves {
namespace {

struct ErrorReporterTest : Test {
  using SrcPath = toml::source_path_ptr;
  const SrcPath expected_source_path =
      std::make_shared<std::string>("source_path");
  const toml::source_region location{.path = expected_source_path};

  using Sut = ErrorReporter;

  Sut sut;
};

TEST_F(ErrorReporterTest, location_is_recorded) {
  sut.location(location);

  // Location is only readable from the exception after throwing.
  auto actual_source_path = SrcPath{};
  try {
    sut.emit_error("");
  } catch (const toml::parse_error& err) {
    actual_source_path = err.source().path;
  }

  ASSERT_EQ(expected_source_path, actual_source_path);
}

TEST_F(ErrorReporterTest, message) {
  const auto expected_message = std::string{"expected_message"};

  // Message is only recorded in thrown exception.
  auto actual_message = std::string_view{};
  try {
    sut.emit_error(expected_message);
  } catch (const toml::parse_error& err) {
    actual_message = err.description();
  }

  ASSERT_EQ(expected_message, actual_message);
}

}  // namespace
}  // namespace curves
