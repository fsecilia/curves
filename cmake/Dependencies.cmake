# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file describes how to find external dependencies.
#
# For bundled deps, it prefers the host version when it is recent enough, falling back to the vendored version
# otherwise. This can be controlled both globally or per dep:
#   - Set USE_HOST_DEPS to True to require all host deps.
#   - Set USE_HOST_${NAME} to True to require a specific host dep.

# ---------------------------------------------------------------------------------------------------------------------
# Host Dependencies
# ---------------------------------------------------------------------------------------------------------------------

# find system threading library
set(THREADS_PREFER_PTHREAD_FLAG True)
find_package(Threads REQUIRED)

# use ccache, if available
find_program(ccache ccache)
if (ccache)
    set(CMAKE_C_COMPILER_LAUNCHER "${ccache}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${ccache}")
    set(CMAKE_C_LINKER_LAUNCHER "${ccache}")
    set(CMAKE_CXX_LINKER_LAUNCHER "${ccache}")
endif()

# find gtest; enable testing if found
set(INSTALL_GTEST OFF)
if(NOT TARGET GTest::gtest)
    # try finding system package normally
    find_package(GTest QUIET)
endif()
if(NOT TARGET GTest::gtest)
    # look where debian puts the source build
    if (EXISTS /usr/src/googletest)
        add_subdirectory(/usr/src/googletest ${CMAKE_BINARY_DIR}/external/googletest EXCLUDE_FROM_ALL)
    endif()
endif()
if(TARGET GTest::gtest)
    set(enable_testing True)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# External Dependencies
# ---------------------------------------------------------------------------------------------------------------------

option(USE_HOST_DEPS "use host dependencies instead of bundled" OFF)

# finds package, perferring host version to bundled submodule
#
# usage: find_or_bundle(dink "0.1.0...<1.0.0")
macro(find_or_bundle NAME VERSION)
    # try finding package with appropriate strictness
    message(CHECK_START "finding ${NAME}")
    string(TOUPPER "${NAME}" _UPPER_NAME)
    if (USE_HOST_DEPS OR USE_HOST_${_UPPER_NAME})
        find_package(${NAME} ${VERSION} CONFIG REQUIRED)
    else()
        find_package(${NAME} ${VERSION} CONFIG QUIET)
    endif()

    if (${NAME}_FOUND)
        message(CHECK_PASS "using system ${NAME} v${${NAME}_VERSION}")
    else()
        message(CHECK_FAIL "system ${NAME} missing or old; using bundled")

        # disable install
        set(${${NAME}_INSTALL} OFF CACHE BOOL "disable install" FORCE)

        # check for empty submodule
        if (NOT EXISTS "${CMAKE_SOURCE_DIR}/external/${NAME}/CMakeLists.txt")
             message(FATAL_ERROR
                "submodule 'external/${NAME}' missing or empty; "
                "run: git submodule update --init --recursive")
        endif()

        add_subdirectory(external/${NAME} EXCLUDE_FROM_ALL)
    endif()
endmacro()

find_or_bundle(dink "0.1.0...<1.0.0")
find_or_bundle(tomlplusplus "3.4.0...<4.0.0")
