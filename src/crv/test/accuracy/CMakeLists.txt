# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# fixed-point accuracy tests

if (BUILD_INTEGRATION_TESTS)
    add_executable(accuracy_exp2
        accuracy_test_runner.hpp
        exp2.cpp
    )
    target_link_libraries(accuracy_exp2 PRIVATE lib float128)
    set_target_properties(accuracy_exp2 PROPERTIES CXX_EXTENSIONS TRUE)

    add_executable(accuracy_rexp2m1
        accuracy_test_runner.hpp
        exp2_neg_m1.cpp
    )
    target_link_libraries(accuracy_rexp2m1 PRIVATE lib float128)
    set_target_properties(accuracy_rexp2m1 PROPERTIES CXX_EXTENSIONS TRUE)

    add_executable(accuracy_rsqrt
        accuracy_test_runner.hpp
        rsqrt.cpp
    )
    target_link_libraries(accuracy_rsqrt PRIVATE lib float128)
    set_target_properties(accuracy_rsqrt PROPERTIES CXX_EXTENSIONS TRUE)
endif()