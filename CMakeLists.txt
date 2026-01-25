# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# root cmake
#
# This file establishes the build environment, global toolchain settings, dependencies, and naming contracts shared
# between packaging, kernel modules, and user-space targets.

cmake_minimum_required(VERSION 3.28)
project(curves VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectPolicies)
include(ProjectOptions)
include(Dependencies)
include(KernelManifest)
include(Installation)

add_subdirectory(dist)
add_subdirectory(src/crv/kernel)
add_subdirectory(src/crv/ui)
