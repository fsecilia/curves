# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    command/command.hpp
    command/command.cpp
    command/stack_adapter.hpp
    command/mutate_param.hpp
    hierarchical_inspector.hpp
)
target_link_libraries(ui PUBLIC lib)

add_executable(ui_test
    command/stack_adapter_test.cpp
    command/mutate_param_test.cpp
    hierarchical_inspector_test.cpp
)
target_link_libraries(ui_test PRIVATE ui lib testing)
gtest_discover_tests(ui_test)

add_subdirectory(qt)
