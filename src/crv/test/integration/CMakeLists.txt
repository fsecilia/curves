# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# longer-running integration tests

option(BUILD_INTEGRATION_TESTS "Build longer-running integration tests" OFF)

if (BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests)
    target_sources(integration_tests PRIVATE
        integration_test.hpp
    )
    target_link_libraries(integration_tests PRIVATE lib test)
    gtest_discover_tests(integration_tests)

    # Try finding quadmath.
    find_library(libquadmath quadmath)
    if(libquadmath)
        # The library is typically installed to the normal library location, but the header is technially private to
        # gcc. Try finding it globally first, then ask gcc where to find it.
        find_path(libquadmath_include NAMES quadmath.h)
        if (NOT libquadmath_include)
            execute_process(
                COMMAND gcc -print-file-name=include
                OUTPUT_VARIABLE gcc_private_include_path
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            find_path(libquadmath_include
                NAMES quadmath.h
                HINTS "${gcc_private_include_path}"
                NO_DEFAULT_PATH
            )
        endif()

        # Add dependency if header was found.
        if (libquadmath_include)
            set(libquadmath_FOUND TRUE)
            target_sources(integration_tests PRIVATE
                math/float128_test.cpp
                math/float128.cpp
                math/float128.hpp
            )
            target_link_libraries(integration_tests PRIVATE "${libquadmath}")
            target_include_directories(integration_tests PRIVATE "${libquadmath_include}")
            set_target_properties(integration_tests PROPERTIES CXX_EXTENSIONS TRUE)
        endif()
    endif()

    if (!libquadmath_FOUND)
        message(WARNING "libquadmath not found: 128-bit floating-point accuracy tests excluded")
    endif()
endif()