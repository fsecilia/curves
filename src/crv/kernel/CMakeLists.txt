# SPDX-License-Identifier: GPL-2.0+ OR MIT
# Copyright (c) 2026 Frank Secilia
#
# kernel module build

cmake_minimum_required(VERSION 3.31.6)
project(curves_kernel LANGUAGES C CXX)

foreach(required IN ITEMS CRV_VERSION)
    if(NOT DEFINED "${required}" OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

add_library(driver OBJECT EXCLUDE_FROM_ALL)
target_compile_definitions(driver PRIVATE "$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(driver PRIVATE
        -Wno-keyword-macro
        -Wno-reserved-identifier
        -Wno-variadic-macros
    )
endif()

target_sources(driver PRIVATE
    abi.c
    cxx/cassert
    cxx_build_test.cpp
    cxx_build_test.h
    entry_point.c
    input/handler.c
    input/handler.h
)

include(../config.cmake)
configure_shared(driver PRIVATE)

target_include_directories(driver PRIVATE
    cxx
    ../..
)

set_property(
    SOURCE entry_point.c
    TARGET_DIRECTORY driver
    APPEND PROPERTY COMPILE_DEFINITIONS
        "CRV_VERSION=${CRV_VERSION}"
)
