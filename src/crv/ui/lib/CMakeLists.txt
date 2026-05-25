# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    config/serialization/toml/error_reporter.hpp
)
target_link_libraries(ui PUBLIC lib tomlplusplus::tomlplusplus)

add_executable(ui_test
    config/serialization/toml/error_reporter_test.cpp
)
target_link_libraries(ui_test PRIVATE ui lib testing)
gtest_discover_tests(ui_test)
