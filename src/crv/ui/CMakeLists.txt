# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    command/command.hpp
)
target_link_libraries(ui PUBLIC lib)

# add_executable(ui_test
# )
# target_link_libraries(ui_test PRIVATE ui lib testing)
# gtest_discover_tests(ui_test)

add_subdirectory(qt)