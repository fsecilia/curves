# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#

add_executable(preprod_integration_tests
  isqrt_gamut_tests.cpp
  isqrt_programmatic_tests.cpp
  shaped_spline_accuracy_test.cpp
  synchronous_curve_approximation_accuracy.cpp
  synchronous_curve_asymptote_search.cpp
)
target_link_libraries(preprod_integration_tests PUBLIC
  preprod_lib
  preprod_testing_lib
)
configure_test_target_warnings(preprod_integration_tests)
gtest_discover_tests(preprod_integration_tests)
