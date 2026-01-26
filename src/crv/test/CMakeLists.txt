# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# testing library

add_library(test
    test.cpp
    test.hpp
)
target_link_libraries(test PUBLIC
    GTest::gmock_main
    GTest::gtest_main
    GTest::gmock
    GTest::gtest
)
