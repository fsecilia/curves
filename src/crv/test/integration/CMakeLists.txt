# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# integration tests

if (BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests
        math/stats/percentile_calculator.cpp
        model/shaping/compact_output_limiter_transition.cpp
        model/shaping/output_limited_curve.cpp
    )
    target_link_libraries(integration_tests PRIVATE lib testing)
    add_test(NAME integration_tests COMMAND integration_tests)
    add_dependencies(build_tests integration_tests)
endif()