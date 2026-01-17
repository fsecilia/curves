# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#
# This script defines common testing utilities used by both the unit tests and
# various integration tests.

add_library(testing_lib
  error_metrics.hpp
  isqrt.cpp
  isqrt.hpp
  linear_curve.hpp
  shaped_spline_accuracy.hpp
  test.cpp
  test.hpp
)
target_link_libraries(testing_lib PUBLIC
    lib
    GTest::gmock_main
    GTest::gtest_main
    GTest::gmock
    GTest::gtest
)

add_subdirectory(integration)
