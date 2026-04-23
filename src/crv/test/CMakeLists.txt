# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# testing library

option(BUILD_INTEGRATION_TESTS "build longer-running integration tests" OFF)

add_library(testing
    test.cpp
    test.hpp
    typed_equal.hpp
)
target_link_libraries(testing PUBLIC
    GTest::gmock_main
    GTest::gtest_main
    GTest::gmock
    GTest::gtest
)

if(ENABLE_COVERAGE)
    target_compile_options(testing PUBLIC --coverage)
    target_link_options(testing PUBLIC --coverage)
endif()

# make death tests optional
option(RUN_DEATH_TESTS "run slow death tests when unit testing" ON)
if (RUN_DEATH_TESTS)
    target_compile_definitions(testing PUBLIC "-DCRV_ENABLE_DEATH_TESTS")
endif()

add_subdirectory(accuracy)
add_subdirectory(float128)
add_subdirectory(integration)
add_subdirectory(performance)
