# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# testing support and analysis programs

option(BUILD_ANALYSIS_TESTS "build named performance and accuracy tests" OFF)

if (BUILD_TESTING)
    add_library(testing
        mock_callable.hpp
        test.cpp
        test.hpp
        typed_equal.hpp
    )
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(testing PUBLIC -Wno-self-move)
    endif()
    target_link_libraries(testing PUBLIC
        GTest::gmock_main
        GTest::gtest_main
        GTest::gmock
        GTest::gtest
    )

    if (ENABLE_COVERAGE)
        target_compile_options(testing PUBLIC --coverage)
        target_link_options(testing PUBLIC --coverage)
    endif()

    # make death tests optional
    option(RUN_DEATH_TESTS "run death tests when testing" ON)
    if (RUN_DEATH_TESTS)
        target_compile_definitions(testing PUBLIC "-DCRV_ENABLE_DEATH_TESTS")
    endif()
endif()

if (BUILD_TESTING OR BUILD_ANALYSIS_TESTS)
    add_subdirectory(float128)
endif()

if (BUILD_ANALYSIS_TESTS)
    add_subdirectory(accuracy)
    add_subdirectory(performance)
endif()
