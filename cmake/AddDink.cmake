# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

set(DINK_MIN_VERSION "0.1.0")
set(DINK_MAX_VERSION "1.0.0")

if (NOT DEFINED USE_SYSTEM_DINK)
  if(DEFINED USE_SYSTEM_DEPS)
    set(USE_SYSTEM_DINK ${USE_SYSTEM_DEPS})
  else()
    set(USE_SYSTEM_DINK OFF)
  endif()
endif()

message(CHECK_START "Finding Dink")

if (USE_SYSTEM_DINK)
  # Maintainer mode depends strongly on system library. Uses REQUIRED.
  find_package(DINK
    ${DINK_MIN_VERSION}...<${DINK_MAX_VERSION} CONFIG REQUIRED)
  message(CHECK_PASS "System (Found v${DINK_VERSION})")
else()
  # Developer mode. Uses submodule if configured. Falls back to it if system
  # library not found.
  find_package(DINK
    ${DINK_MIN_VERSION}...<${DINK_MAX_VERSION} CONFIG QUIET)

  if(DINK_FOUND)
    message(CHECK_PASS "System (Found v${DINK_VERSION})")
  else()
    message(CHECK_FAIL "System missing or old. Using Bundled.")

    set(DINK_SUBMODULE_PATH "${CMAKE_SOURCE_DIR}/external/dink")

    # Make sure submodules are init.
    if(NOT EXISTS "${DINK_SUBMODULE_PATH}/CMakeLists.txt")
      message(FATAL_ERROR
        "Submodule 'external/dink' is missing or empty.\n"
        "Run: git submodule update --init --recursive"
      )
    endif()

    # Prevent library from adding its own install targets.
    set(DINK_INSTALL OFF CACHE BOOL "" FORCE)

    # Add subdirectory.
    add_subdirectory(${DINK_SUBMODULE_PATH}
      "${CMAKE_BINARY_DIR}/external/dink")
  endif()
endif()
