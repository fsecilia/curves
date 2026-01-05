# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Frank Secilia
#
# This script defines the project library and kicks off the driver's Kbuild
# when necessary.
#
# The Kbuild is driven by `driver/Kbuild`.

# -----------------------------------------------------------------------------
# Project Files
# -----------------------------------------------------------------------------

list(APPEND lib_files
  config/curve.hpp
  config/enum.hpp
  config/param.hpp
  config/profile.hpp
  config/profile_store.hpp
  config/serialization/reader.hpp
  config/serialization/toml/error_reporter.hpp
  config/serialization/toml/reader_adapter.hpp
  config/serialization/toml/writer_adapter.hpp
  config/serialization/writer.hpp
  curves/synchronous.hpp
  curves/transfer_function/curve.hpp
  curves/transfer_function/from_gain.hpp
  curves/transfer_function/from_sensitivity.hpp
  lib.cpp
  lib.hpp
  math/input_shaping.hpp
  math/input_shaping.cpp
  math/input_shaping_view.hpp
  math/curve.hpp
  math/curve_view.hpp
  math/fixed.hpp
  math/integration.hpp
  math/inverse_function.hpp
  math/io.hpp
  math/jet.hpp
  math/segment/construction.hpp
  math/segment/packing.hpp
  math/segment/segment.hpp
  math/segment/view.hpp
  math/spline.cpp
  math/spline.hpp
  math/spline_view.hpp
  numeric_cast.hpp
  ui/model/flat_visitor.hpp
  ui/model/view_model.hpp
  ui/rendering/spline_evaluator.hpp
  ui/rendering/spline_sampler.hpp
)

list(APPEND unit_tests_files
  config/curve_test.cpp
  config/enum_test.cpp
  config/param_test.cpp
  config/profile_store_test.cpp
  config/profile_test.cpp
  config/serialization/reader_test.cpp
  config/serialization/toml/error_reporter_test.cpp
  config/serialization/toml/reader_adapter_test.cpp
  config/serialization/toml/writer_adapter_test.cpp
  config/serialization/writer_test.cpp
  curves/synchronous_test.cpp
  curves/transfer_function/curve_test.cpp
  curves/transfer_function/from_gain_test.cpp
  curves/transfer_function/from_sensitivity_test.cpp
  driver_tests/fixed/add.cpp
  driver_tests/fixed/divide.cpp
  driver_tests/fixed/exp2.cpp
  driver_tests/fixed/isqrt.cpp
  driver_tests/fixed/log2.cpp
  driver_tests/fixed/multiply.cpp
  driver_tests/fixed/s128.cpp
  driver_tests/fixed/s64.cpp
  driver_tests/fixed/subtract.cpp
  driver_tests/fixed/test.cpp
  driver_tests/int_math_test.cpp
  math/curve_test.cpp
  math/curve_view_test.cpp
  math/input_shaping_test.cpp
  math/input_shaping_view_test.cpp
  math/integration_test.cpp
  math/inverse_function_test.cpp
  math/io_test.cpp
  math/segment/construction_test.cpp
  math/segment/packing_test.cpp
  math/segment/view_test.cpp
  math/spline_test.cpp
  math/spline_view_test.cpp
  ui/model/flat_visitor_test.cpp
  ui/model/view_model_test.cpp
  ui/rendering/spline_evaluator_test.cpp
  ui/rendering/spline_sampler_test.cpp
)

# The Kbuild is the source of truth for the files in the driver. CMake asks the
# Kbuild for two lists of .c files:
#
#   1. Driver Sources: Files specific to the driver (e.g., `entry_point.c`).
#      These are only built by the Kbuild.
#   2. Common Sources: Files shared between the driver and library (e.g.,
#      `math64.c`). These are built by both the Kbuild for the driver and by
#      CMake in the library.
#
# These common sources are c implementations of things like fixed point math
# and the individual sensitivity curves. They are not anything that crosses the
# UAPI.
#
# The CMake custom command that kicks off the Kbuild depends on both driver
# sources and common sources. If any of these are modified, the Kbuild is run
# automatically. However, this leaves a dependency gap: the Kbuild only reports
# sources; it does not report headers.
#
# The list of headers must be tracked manually. The list is split into 2 parts,
# headers specific to the driver, and headers common to both the driver and the
# library. This way, the common headers can be added to the library, and the
# Kbuild target can depend on both.

list(APPEND driver_headers
  driver/entry_point.h
)

list(APPEND common_headers
  driver/fixed.h
  driver/input_shaping.h
  driver/int_math.h
  driver/kernel_compat.h
  driver/segment/eval.h
  driver/segment/unpacking.h
  driver/spline.h
)

# Driver .c files manually included in .cpp files.
list(APPEND included_driver_source_files
  driver/input_shaping.c
  driver/spline.c
)

list(APPEND kbuild_files
  driver/Kbuild
  driver/Kconfig
)

# -----------------------------------------------------------------------------
# Driver Paths
# -----------------------------------------------------------------------------

# Define input and output dirs.
set(driver_bin_dir "${CMAKE_CURRENT_BINARY_DIR}/driver")
set(driver_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/driver")

# -----------------------------------------------------------------------------
# Kbuild Export
# -----------------------------------------------------------------------------

# Write version.mk for the Kbuild.
#
# This is how we export the CMake build version to the driver.
file(WRITE
  "${driver_src_dir}/version.mk" "CURVES_VERSION := ${PROJECT_VERSION}\n"
)

# -----------------------------------------------------------------------------
# Kbuild Import
# -----------------------------------------------------------------------------

# Get common sources from Kbuild.
execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo "Getting common sources..."
  COMMAND make -f kbuild_query.mk print-curves-common-sources
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE common_sources
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(common_sources UNIX_COMMAND ${common_sources})

# Get driver sources from Kbuild.
execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo "Getting driver sources..."
  COMMAND make -f kbuild_query.mk print-curves-driver-sources
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE driver_sources
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(driver_sources UNIX_COMMAND ${driver_sources})

# -----------------------------------------------------------------------------
# Kbuild Delegation
# -----------------------------------------------------------------------------

# Path to final build output.
set(driver_output "${driver_bin_dir}/curves.ko")

# Define base make command with no target listed.
set(driver_make_command make -sC "/lib/modules/${CMAKE_SYSTEM_VERSION}/build"
    M=${driver_src_dir}
    MO=${driver_bin_dir}
)

# Kick off Kbuild.
add_custom_command(
  OUTPUT "${driver_output}"
  COMMAND ${driver_make_command} modules
  WORKING_DIRECTORY "${driver_bin_dir}"
  DEPENDS
    ${kbuild_files}
    ${driver_sources}
    ${driver_headers}
    ${common_sources}
    ${common_headers}
  COMMENT "Building driver..."
  VERBATIM
  USES_TERMINAL
)

# Add named target to kick off Kbuild.
add_custom_target(driver ALL DEPENDS "${driver_output}")

# Add named target for cleaning the Kbuild.
add_custom_target(clean_driver
  COMMAND ${driver_make_command} clean
  WORKING_DIRECTORY "${driver_bin_dir}"
  COMMENT "Cleaning driver..."
  VERBATIM
  USES_TERMINAL
)

# -----------------------------------------------------------------------------
# Generated Config
# -----------------------------------------------------------------------------

set(config_template_file config.hpp.in)
set(config_gen_filename config.gen.hpp)
set(config_gen_root "${CMAKE_BINARY_DIR}/gen")
set(config_gen_file "${config_gen_root}/curves/${config_gen_filename}")
configure_file("${config_template_file}" "${config_gen_file}")

# -----------------------------------------------------------------------------
# Library Project
# -----------------------------------------------------------------------------

add_library(lib
  ${common_sources}
  ${common_headers}
  ${lib_files}
  "${config_template_file}"
  "${config_gen_file}"
)

target_include_directories(lib PUBLIC
  "$<BUILD_INTERFACE:${config_gen_root}>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/curves/driver>"
)
target_link_libraries(lib PUBLIC tomlplusplus::tomlplusplus)

set_target_properties(lib
  PROPERTIES
    OUTPUT_NAME curves
    VERSION ${PROJECT_VERSION}
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

# Disable warnings from building kernel C files in C++ mode.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(lib PUBLIC
    # This is temporary until we get a few modules populated.
    -Wno-empty-translation-unit

    -Wno-gnu-statement-expression-from-macro-expansion
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-language-extension-token
    -Wno-old-style-cast
    -Wno-reserved-identifier
    -Wno-reserved-macro-identifier
    -Wno-shadow-field-in-constructor
  )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(lib PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
  )
endif()

install(TARGETS lib
  # Install .a or .lib, if configured.
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}

  # Install .so, if configured.
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}

  # Install .dll, if configured.
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# -----------------------------------------------------------------------------
# Test Project
# -----------------------------------------------------------------------------

if (enable_testing)
  add_subdirectory(testing)

  add_executable(unit_tests ${unit_tests_files})
  target_link_libraries(unit_tests PUBLIC
    lib
    testing_lib
  )
  configure_test_target_warnings(unit_tests)
  gtest_discover_tests(unit_tests)

  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(unit_tests PUBLIC
      -Wno-unused-function
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Kernel Files Lib
# -----------------------------------------------------------------------------

# There are some kernel functions that are, within the context of the kernel,
# private. They are called by only the module they are defined in, so they must
# be made static. However, some of these are also called from c++. We would
# normally put their prototypes in a header, but because they are static, they
# won't be found at link time, prototypes or no.
#
# Idiomatically in the kernel, for unit testing, those private functions are
# made public by #including the .c file. This is about the best we're going to
# get from c++: include the c file, then wrap it in a namespaced function. Then
# the static functions remain private to the module, the public functions are
# exported correctly in both the kernel and user mode. The problem is, IDEs
# won't see the .c file this way, so clang refactoring and global find and
# replace stop working.
#
# We can't just add the the .c file to the project along side the .cpp that
# includes it, because the module's public symbols would conflict. The least
# bad solution I've found to get search working is to also make a separate
# library containing the .c files that is not linked by anything. Then the
# kernel is happy because everything not exported is private, c++ is happy
# becasue it can see the static definitions, and the IDE is happy because we
# have the .c in compile_commands.json again.
#
# This is that library.
add_library(kernel_lib OBJECT ${included_driver_source_files})
target_link_libraries(kernel_lib PRIVATE lib)

# -----------------------------------------------------------------------------
# Project Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(ui)
