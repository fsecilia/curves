// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Main test header.

  This file is included by every test file.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <algorithm>
#include <gtest/gtest.h>
#include <string>

namespace curves {

using namespace testing;

// Name generator for readable test output.
template <typename TestVector>
struct TestNameGenerator {
  auto operator()(const TestParamInfo<TestVector>& info) const -> std::string {
    auto s = std::string{info.param.description};
    std::replace_if(
        s.begin(), s.end(), [](char c) { return !std::isalnum(c); }, '_');
    return s;
  }
};

}  // namespace curves
