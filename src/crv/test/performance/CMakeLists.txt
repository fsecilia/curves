# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# various performance tests

if (BUILD_INTEGRATION_TESTS)
    add_executable(performance_test_shifted_int_divider
        performance.hpp
        shifted_int_divider.cpp
    )
    target_link_libraries(performance_test_shifted_int_divider PRIVATE lib)
endif()