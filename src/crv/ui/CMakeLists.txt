# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    serialization/exceptions.hpp
    serialization/toml/error_reporter.hpp
    reflection/constraints.hpp
    reflection/enum.hpp
    reflection/param.hpp
)
target_link_libraries(ui PUBLIC lib tomlplusplus::tomlplusplus)

add_executable(ui_test
    serialization/exceptions.cpp
    serialization/toml/error_reporter_test.cpp
    reflection/constraints_test.cpp
    reflection/enum_test.cpp
    reflection/param_test.cpp
)
target_link_libraries(ui_test PRIVATE ui lib testing)
gtest_discover_tests(ui_test)

add_subdirectory(qt)