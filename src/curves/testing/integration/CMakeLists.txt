# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#

add_executable(integration_tests
  isqrt_gamut_tests.cpp
  isqrt_programmatic_tests.cpp
  synchronous_curve_asymptote_search.cpp
)
target_link_libraries(integration_tests PUBLIC
  lib
  testing_lib
)
configure_test_target_warnings(integration_tests)
gtest_discover_tests(integration_tests)
