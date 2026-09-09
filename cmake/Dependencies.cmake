# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# describes how to find external dependencies
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

# ---------------------------------------------------------------------------------------------------------------------
# External Dependencies
# ---------------------------------------------------------------------------------------------------------------------

# finds a dependency, preferring an initialized bundled submodule
#
# usage:
#   find_or_bundle(tomlplusplus "3.4.0...<4.0.0")
#   find_or_bundle(
#       GTest "1.18.0...<2.0.0"
#       BUNDLE googletest
#       REQUIRED_TARGETS
#           GTest::gtest
#           GTest::gtest_main
#           GTest::gmock
#           GTest::gmock_main
#   )
function(find_or_bundle NAME VERSION)
    cmake_parse_arguments(
        ARG
        ""
        "BUNDLE"
        "REQUIRED_TARGETS"
        ${ARGN}
    )

    if (ARG_BUNDLE)
        set(_bundle_name "${ARG_BUNDLE}")
    else()
        set(_bundle_name "${NAME}")
    endif()

    set(_bundle_dir "${CMAKE_SOURCE_DIR}/external/${_bundle_name}")

    message(CHECK_START "finding ${NAME}")

    if (EXISTS "${_bundle_dir}/CMakeLists.txt")
        message(CHECK_PASS "using bundled ${_bundle_name}")
        add_subdirectory("${_bundle_dir}" EXCLUDE_FROM_ALL)
    else()
        find_package(${NAME} "${VERSION}" CONFIG QUIET)

        if (${NAME}_FOUND)
            message(CHECK_PASS "using system ${NAME} v${${NAME}_VERSION}")
        else()
            message(CHECK_FAIL "compatible system ${NAME} not found")
            message(FATAL_ERROR
                "dependency '${NAME}' unavailable; initialize submodule "
                "'external/${_bundle_name}' or install a compatible host package")
        endif()
    endif()

    foreach (_target IN LISTS ARG_REQUIRED_TARGETS)
        if (NOT TARGET "${_target}")
            message(FATAL_ERROR "dependency '${NAME}' did not provide required target '${_target}'")
        endif()
    endforeach()
endfunction()

set(tomlplusplus_INSTALL OFF CACHE BOOL "install tomlplusplus transitively")
find_or_bundle(tomlplusplus "3.4.0...<4.0.0")

if (BUILD_TESTING)
    set(INSTALL_GTEST OFF CACHE BOOL "install gtest transitively")
    find_or_bundle(
        GTest "1.16.0...<2.0.0"
        BUNDLE gtest
        REQUIRED_TARGETS
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
    )
endif()
