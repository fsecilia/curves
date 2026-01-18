# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

# 3.4 was current on debian stable (trixie) at the time of development.
set(TOML_MIN_VERSION "3.4.0")
set(TOML_MAX_VERSION "4.0.0")

if (NOT DEFINED USE_SYSTEM_TOMLPLUSPLUS)
  if(DEFINED USE_SYSTEM_DEPS)
    set(USE_SYSTEM_TOMLPLUSPLUS ${USE_SYSTEM_DEPS})
  else()
    set(USE_SYSTEM_TOMLPLUSPLUS OFF)
  endif()
endif()

message(CHECK_START "Finding tomlplusplus")

if (USE_SYSTEM_TOMLPLUSPLUS)
  # Maintainer mode depends strongly on system library. Uses REQUIRED.
  find_package(tomlplusplus ${TOML_MIN_VERSION}...<${TOML_MAX_VERSION} CONFIG REQUIRED)
  message(CHECK_PASS "System (Found v${tomlplusplus_VERSION})")
else()
  # Developer mode uses submodule if configured. Falls back to it if system library not found.
  find_package(tomlplusplus ${TOML_MIN_VERSION}...<${TOML_MAX_VERSION} CONFIG QUIET)

  if(tomlplusplus_FOUND)
    message(CHECK_PASS "System (Found v${tomlplusplus_VERSION})")
  else()
    message(CHECK_FAIL "System missing or old. Using Bundled.")

    set(TOML_SUBMODULE_PATH "${CMAKE_SOURCE_DIR}/external/tomlplusplus")

    # Make sure submodules are init.
    if(NOT EXISTS "${TOML_SUBMODULE_PATH}/CMakeLists.txt")
      message(FATAL_ERROR
        "Submodule 'external/tomlplusplus' is missing or empty.\n"
        "Run: git submodule update --init --recursive"
      )
    endif()

    # Prevent library from adding its own install targets.
    set(TOMLPLUSPLUS_INSTALL OFF CACHE BOOL "" FORCE)

    # Add subdirectory.
    add_subdirectory(${TOML_SUBMODULE_PATH} "${CMAKE_BINARY_DIR}/external/tomlplusplus")
  endif()
endif()
