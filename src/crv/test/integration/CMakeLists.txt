# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# integration tetss

if (BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests
        math/stats/percentile_calculator.cpp
    )
    target_link_libraries(integration_tests PRIVATE lib test)
endif()