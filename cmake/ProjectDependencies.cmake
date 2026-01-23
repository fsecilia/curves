# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

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

