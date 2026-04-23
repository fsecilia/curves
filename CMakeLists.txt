# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# root cmake
#
# This file establishes the build environment, global toolchain settings, dependencies, and naming contracts shared
# between packaging, kernel modules, and user-space targets.

cmake_minimum_required(VERSION 3.28)
project(curves VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectPolicies)
include(ProjectOptions)
include(Dependencies)
include(KernelManifest)
include(Installation)

# aggregate target that depends on all tests, so all tests can be built with one target
add_custom_target(build_tests COMMENT "building all test executables...")

option(ENABLE_COVERAGE "enable test coverage reporting" OFF)
if(ENABLE_COVERAGE)
    enable_testing()

    add_custom_target(coverage
        # wipe old coverage data so we get a fresh count
        COMMAND lcov --zerocounters --directory .

        # run the tests automatically
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure

        # capture the new coverage data
        COMMAND lcov --capture --directory . --output-file coverage.info --ignore-errors count --ignore-errors inconsistent --ignore-errors unused

        # filter out system headers and tests
        COMMAND lcov --remove coverage.info '/usr/*' '*/unit_tests/*' '*/test/*' --output-file coverage.info --ignore-errors count --ignore-errors inconsistent --ignore-errors unused

        # generate the HTML report
        COMMAND genhtml coverage.info --output-directory coverage_report --ignore-errors count --ignore-errors inconsistent --ignore-errors unused

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "building, testing, and generating coverage report..."
    )

    add_dependencies(coverage build_tests)
endif()

add_subdirectory(dist)
add_subdirectory(src/crv)
