# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# optional 128-bit floating point support

option(BUILD_INTEGRATION_TESTS "Build longer-running integration tests" OFF)
option(polyfill_float128_exp2 "Define std::exp2(std::float128_t)" ON)
option(polyfill_float128_ldexp "Define std::ldexp(std::float128_t, int)" ON)
option(polyfill_float128_llround "Define std::llround(std::float128_t) polyfill" ON)
option(polyfill_float128_log2 "Define std::log2(std::float128_t)" ON)
option(polyfill_float128_ostream_inserter "Define operator <<(std::ostream&, std::float128_t) polyfill" ON)
option(polyfill_float128_round "Define std::round(std::float128_t) polyfill" ON)
option(polyfill_float128_pow "Define std::pow(std::float128_t) polyfill" ON)
option(polyfill_float128_sqrt "Define std::sqrt(std::float128_t) polyfill" ON)

if (BUILD_INTEGRATION_TESTS)
    # The library is a shell unless float128 is available. By default, it only includes the header to signal to other
    # targets whether float128 is available or not.
    add_library(float128)
    target_sources(float128 PRIVATE float128.hpp)

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
        # add implementation to library
        target_sources(float128 PRIVATE float128.cpp)
        set_target_properties(float128 PROPERTIES CXX_EXTENSIONS TRUE)
        target_compile_definitions(float128 PUBLIC "-DCRV_FEATURE_FLOAT_128")
        target_link_libraries(float128 PUBLIC "${libquadmath}" lib)

        # Normally, the path to libquadmath should be included this way:
        #
        #   target_include_directories(float128 SYSTEM PUBLIC "${libquadmath_include}")
        #
        # However, libquadmath is shipped as part of gcc. When compiling, gcc already implicitly searches this path.
        # CMake knows this, so when building with gcc, cmake does not emit this path to compile_commands.json.
        # However, clangd does not know this, and requires the path.
        #
        # Instead, we manually specify the argument. This way, cmake emits it to compile_commands.json and clangd picks
        # it up there.
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(float128 PUBLIC "SHELL:-isystem ${libquadmath_include}")
        endif()

        # When building with clang, these polyfills must be visible before any imports. This introduces an include order
        # dependency, which is fragile. Here, the polyfills header is explicitly included first from the commandline.
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(float128 PUBLIC "-include${CMAKE_CURRENT_SOURCE_DIR}/float128.hpp")
        endif()

        if (polyfill_float128_exp2)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_EXP2")
        endif()

        if (polyfill_float128_ldexp)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LDEXP")
        endif()

        if (polyfill_float128_llround)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LLROUND")
        endif()

        if (polyfill_float128_log2)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LOG2")
        endif()

        if (polyfill_float128_ostream_inserter)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_OSTREAM_INSERTER")
        endif()

        if (polyfill_float128_pow)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_POW")
        endif()

        if (polyfill_float128_round)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_ROUND")
        endif()

        if (polyfill_float128_sqrt)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_SQRT")
        endif()

        # add test executable
        add_executable(float128_tests)
        target_sources(float128_tests PRIVATE float128_test.cpp)
        target_link_libraries(float128_tests PRIVATE float128 test)
        gtest_discover_tests(float128_tests)
    else()
        message(WARNING "libquadmath not found: 128-bit floating-point unavailable")
    endif()
endif()
