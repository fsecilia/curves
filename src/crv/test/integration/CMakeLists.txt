# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# longer-running integration tests

option(BUILD_INTEGRATION_TESTS "Build longer-running integration tests" OFF)
option(polyfill_float128_exp2 "Define std::exp2(std::float128_t)" ON)
option(polyfill_float128_ldexp "Define std::ldexp(std::float128_t, int)" ON)
option(polyfill_float128_log2 "Define std::log2(std::float128_t)" ON)
option(polyfill_float128_ostream_inserter "Define operator <<(std::ostream&, std::float128_t) polyfill" ON)
option(polyfill_float128_round "Define std::round(std::float128_t) polyfill" ON)
option(polyfill_float128_sqrt "Define std::sqrt(std::float128_t) polyfill" ON)

if (BUILD_INTEGRATION_TESTS)
    add_executable(integration_tests)
    target_sources(integration_tests PRIVATE
        math/exp2_test.cpp
        math/float128.hpp
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
        endif()
    endif()

    if (libquadmath_FOUND)
        target_sources(integration_tests PRIVATE
            math/float128_test.cpp
            math/float128.cpp
        )
        target_link_libraries(integration_tests PRIVATE "${libquadmath}")
        target_include_directories(integration_tests SYSTEM PRIVATE "${libquadmath_include}")
        set_target_properties(integration_tests PROPERTIES CXX_EXTENSIONS TRUE)

        target_compile_definitions(integration_tests PRIVATE "-DCRV_FEATURE_FLOAT_128")

        if (polyfill_float128_exp2)
            target_compile_definitions(integration_tests PRIVATE "-DCRV_POLYFILL_FLOAT128_EXP2")
        endif()

        if (polyfill_float128_ldexp)
            target_compile_definitions(integration_tests PRIVATE "-DCRV_POLYFILL_FLOAT128_LDEXP")
        endif()

        if (polyfill_float128_log2)
            target_compile_definitions(integration_tests PRIVATE "-DCRV_POLYFILL_FLOAT128_LOG2")
        endif()

        if (polyfill_float128_ostream_inserter)
            target_compile_definitions(integration_tests PRIVATE "-DCRV_POLYFILL_FLOAT128_OSTREAM_INSERTER")
        endif()

        if (polyfill_float128_round)
            target_compile_definitions(integration_tests PRIVATE "-DCRV_POLYFILL_FLOAT128_ROUND")
        endif()

        if (polyfill_float128_sqrt)
            target_compile_definitions(integration_tests PRIVATE "-DCRV_POLYFILL_FLOAT128_SQRT")
        endif()

        # When buliding with clang, the polyfills must be visible before any imports. This introduces an include order
        # dependency, which is fragile. Here, the polyfills header is explicitly included first from the commandline.
        target_compile_options(integration_tests PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/math/float128.hpp"
        )
    else()
        message(WARNING "libquadmath not found: 128-bit floating-point unavailable")
    endif()
endif()
