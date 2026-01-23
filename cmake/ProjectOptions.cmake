# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# This file declares compile and link options and an interface library containing them that other targets can link to.

# ---------------------------------------------------------------------------------------------------------------------
# Standardized Build Settings
# ---------------------------------------------------------------------------------------------------------------------

# c++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_SCAN_FOR_MODULES FALSE)

# visibility
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# lto
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# ---------------------------------------------------------------------------------------------------------------------
# Build Options
# ---------------------------------------------------------------------------------------------------------------------

# collects options common to user mode and kernel mode
list(APPEND compile_options_common)

# collects specific to user mode
list(APPEND compile_options_user_mode)
list(APPEND link_options_user_mode)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND compile_options_common
        -Wall
        -Werror
        -Wextra
        -Wswitch-enum
    )
    list(APPEND compile_options_user_mode
        -fsized-deallocation
        -fstrict-aliasing
        -Wstrict-aliasing=2
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND compile_options_common
        -fsafe-buffer-usage-suggestions

        -Weverything

        -Wno-c++23-compat
        -Wno-c++20-compat
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-c99-extensions
        -Wno-documentation
        -Wno-documentation-unknown-command
        -Wno-exit-time-destructors
        -Wno-global-constructors
        -Wno-padded
        -Wno-switch-default
        -Wno-unsafe-buffer-usage
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(msvc_compile_options_common_release /GS /Gy /Zc:inline)
    list(APPEND compile_options_common
        $<$<CONFIG:RELEASE>:${msvc_compile_options_common_release}>

        -D_CRT_SECURE_NO_WARNINGS
        -D_SCL_SECURE_NO_WARNINGS

        /W4
        /WX
        /utf-8
        /wd4201
        /wd4250
        /wd4251
        /wd4275
        /wd4458
        /wd4459
        /wd4576
    )

    set(msvc_link_flags_release /LTCG /OPT:ICF/OPT:REF)
    list(APPEND link_options_user_mode
        $<$<CONFIG:RELEASE>:${msvc_link_flags_release}>

        /WX
    )
endif()

option(ENABLE_ASAN "Enable AddressSanitizer (ASan) for debug builds." OFF)
if (ENABLE_ASAN)
    list(APPEND compile_options_user_mode -fsanitize=address -fno-omit-frame-pointer)
    list(APPEND link_options_user_mode -fsanitize=address)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Options Library
# ---------------------------------------------------------------------------------------------------------------------

# targets link to this library to get common and user mode options
add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE ${compile_options_common} ${compile_options_user_mode})
target_link_options(project_options INTERFACE ${link_options_user_mode})
