# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    command/command.cpp
    command/command.hpp
    command/mutate_param.hpp
    command/stack.hpp
    command/timeline.hpp
    hierarchical_inspector.hpp
)
target_link_libraries(ui PUBLIC lib)

add_executable(ui_test
    command/command_test.cpp
    command/mutate_param_test.cpp
    command/stack_test.cpp
    command/timeline_test.cpp
    hierarchical_inspector_test.cpp
)
target_link_libraries(ui_test PRIVATE ui lib testing)
gtest_discover_tests(ui_test)

add_executable(ui_integration_test
    command/command_integration_test.cpp
)
target_link_libraries(ui_integration_test PRIVATE ui lib testing)
gtest_discover_tests(ui_integration_test)

add_subdirectory(qt)
