# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    command/command.cpp
    command/command.hpp
    command/mutate_param.hpp
    command/stack.hpp
    command/stack.cpp
    command/timeline.hpp
    hierarchical_inspector.hpp
)
target_link_libraries(ui PUBLIC lib)

if (crv_enable_testing)
    add_executable(ui_regression_tests
        command/command_integration_test.cpp
        command/command_test.cpp
        command/mutate_param_test.cpp
        command/stack_test.cpp
        command/timeline_test.cpp
        hierarchical_inspector_test.cpp
    )
    target_link_libraries(ui_regression_tests PRIVATE ui lib testing)
    add_test(NAME ui_regression_tests COMMAND ui_regression_tests)
    add_dependencies(build_tests ui_regression_tests)
endif()

add_subdirectory(qt)
