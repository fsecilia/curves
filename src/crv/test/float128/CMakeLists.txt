# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# optional 128-bit floating point support

# handle clang ambiguities with thin polyfills
option(polyfill_float128_exp2 "Define std::exp2(std::float128_t)" ON)
option(polyfill_float128_ldexp "Define std::ldexp(std::float128_t, int)" ON)
option(polyfill_float128_llrint "Define std::llrint(std::float128_t) polyfill" ON)
option(polyfill_float128_llround "Define std::llround(std::float128_t) polyfill" ON)
option(polyfill_float128_log2 "Define std::log2(std::float128_t)" ON)
option(polyfill_float128_ostream_inserter "Define operator <<(std::ostream&, std::float128_t) polyfill" ON)
option(polyfill_float128_nearbyint "Define std::nearbyint(std::float128_t) polyfill" ON)
option(polyfill_float128_pow "Define std::pow(std::float128_t) polyfill" ON)
option(polyfill_float128_rint "Define std::rint(std::float128_t) polyfill" ON)
option(polyfill_float128_round "Define std::round(std::float128_t) polyfill" ON)
option(polyfill_float128_sqrt "Define std::sqrt(std::float128_t) polyfill" ON)

if (BUILD_TESTING OR BUILD_ANALYSIS_TESTS)
    # libquadmath often lives in compiler-private directories omitted from ordinary CMake search paths.
    find_library(libquadmath NAMES quadmath)
    find_path(libquadmath_include NAMES quadmath.h)

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if (NOT libquadmath)
            execute_process(
                COMMAND "${CMAKE_CXX_COMPILER}" "-print-file-name=libquadmath${CMAKE_SHARED_LIBRARY_SUFFIX}"
                RESULT_VARIABLE libquadmath_query_result
                OUTPUT_VARIABLE compiler_libquadmath
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if (libquadmath_query_result EQUAL 0 AND EXISTS "${compiler_libquadmath}")
                set(libquadmath "${compiler_libquadmath}")
            endif()
        endif()

        if (NOT libquadmath)
            execute_process(
                COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libquadmath.a
                RESULT_VARIABLE libquadmath_query_result
                OUTPUT_VARIABLE compiler_libquadmath
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if (libquadmath_query_result EQUAL 0 AND EXISTS "${compiler_libquadmath}")
                set(libquadmath "${compiler_libquadmath}")
            endif()
        endif()

        if (NOT libquadmath_include)
            execute_process(
                COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=include/quadmath.h
                RESULT_VARIABLE libquadmath_include_query_result
                OUTPUT_VARIABLE compiler_quadmath_header
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if (libquadmath_include_query_result EQUAL 0 AND EXISTS "${compiler_quadmath_header}")
                get_filename_component(libquadmath_include "${compiler_quadmath_header}" DIRECTORY)
            endif()
        endif()
    endif()

    if (libquadmath AND libquadmath_include)
        add_library(float128 float128.cpp)
        set_target_properties(float128 PROPERTIES CXX_EXTENSIONS TRUE)
        target_compile_definitions(float128 PUBLIC "-DCRV_FEATURE_FLOAT_128")
        target_link_libraries(float128 PUBLIC "${libquadmath}" lib)

        # Normally, the path to libquadmath should be included this way:
        #
        #   target_include_directories(float128 SYSTEM PUBLIC "${libquadmath_include}")
        #
        # However, libquadmath is shipped as part of gcc. When compiling, gcc already implicitly searches this path.
        # CMake knows this, so when building with gcc, cmake does not emit this path to compile_commands.json. However,
        # clangd does not know this and requires the path.
        #
        # Instead, specify the argument manually so CMake emits it to compile_commands.json for clangd.
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(float128 PUBLIC "SHELL:-isystem ${libquadmath_include}")
        endif()

        # These polyfills must be visible before any imports. This introduces an include order dependency, which is
        # fragile. Here, the polyfills header is explicitly included first from the commandline.
        target_compile_options(float128 PUBLIC "-include${CMAKE_CURRENT_SOURCE_DIR}/float128.hpp")

        if (polyfill_float128_exp2)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_EXP2")
        endif()

        if (polyfill_float128_ldexp)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LDEXP")
        endif()

        if (polyfill_float128_llrint)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LLRINT")
        endif()

        if (polyfill_float128_llround)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LLROUND")
        endif()

        if (polyfill_float128_log2)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_LOG2")
        endif()

        if (polyfill_float128_nearbyint)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_NEARBYINT")
        endif()

        if (polyfill_float128_ostream_inserter)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_OSTREAM_INSERTER")
        endif()

        if (polyfill_float128_pow)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_POW")
        endif()

        if (polyfill_float128_rint)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_RINT")
        endif()

        if (polyfill_float128_round)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_ROUND")
        endif()

        if (polyfill_float128_sqrt)
            target_compile_definitions(float128 PUBLIC "-DCRV_POLYFILL_FLOAT128_SQRT")
        endif()

        if (BUILD_TESTING)
            add_executable(float128_tests float128_test.cpp)
            set_target_properties(float128_tests PROPERTIES CXX_EXTENSIONS TRUE)
            target_link_libraries(float128_tests PRIVATE float128 testing)
            add_test(NAME float128_tests COMMAND float128_tests)
            set_tests_properties(float128_tests PROPERTIES LABELS regression)
            add_dependencies(build_tests float128_tests)
        endif()
    else()
        message(WARNING "quadmath development files not found: float128 support unavailable")
    endif()
endif()
