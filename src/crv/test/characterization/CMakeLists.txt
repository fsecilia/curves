# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# fixed-point characterization tests

if (BUILD_INTEGRATION_TESTS)
    add_executable(characterization_spline
        characterization_spline.cpp
    )
    target_link_libraries(characterization_spline PRIVATE lib)
endif()