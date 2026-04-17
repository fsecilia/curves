# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# testing library

option(BUILD_INTEGRATION_TESTS "build longer-running integration tests" OFF)

add_library(test
    test.cpp
    test.hpp
    typed_equal.hpp
)
target_link_libraries(test PUBLIC
    GTest::gmock_main
    GTest::gtest_main
    GTest::gmock
    GTest::gtest
)

# make death tests optional
option(RUN_DEATH_TESTS "run slow death tests when unit testing" ON)
if (RUN_DEATH_TESTS)
    target_compile_definitions(test PUBLIC "-DCRV_ENABLE_DEATH_TESTS")
endif()

add_subdirectory(accuracy)
add_subdirectory(characterization)
add_subdirectory(float128)
add_subdirectory(integration)
add_subdirectory(performance)
