# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia

cmake_minimum_required(VERSION 4.0.2)
project(curves VERSION 0.1.0)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

# Use ccache, if available.
find_program(ccache ccache)
if (ccache)
  set(CMAKE_C_COMPILER_LAUNCHER "${ccache}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${ccache}")
  set(CMAKE_C_LINKER_LAUNCHER "${ccache}")
  set(CMAKE_CXX_LINKER_LAUNCHER "${ccache}")
endif()

# Find system threading library.
set(THREADS_PREFER_PTHREAD_FLAG True)
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# Project Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(src/curves)
