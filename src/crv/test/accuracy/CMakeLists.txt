# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# fixed-point accuracy tests

if (BUILD_INTEGRATION_TESTS)
    add_executable(accuracy_tests
        exp2.cpp
        exp2_neg_m1.cpp
    )
    target_link_libraries(accuracy_tests PRIVATE lib float128)
    set_target_properties(accuracy_tests PROPERTIES CXX_EXTENSIONS TRUE)
endif()