# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# longer-running integration tests

option(BUILD_INTEGRATION_TESTS "Build longer-running integration tests" OFF)

if (BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests)
    target_sources(integration_tests PRIVATE
        integration_test.hpp
    )
    target_link_libraries(integration_tests PRIVATE lib test)
    gtest_discover_tests(integration_tests)
endif()