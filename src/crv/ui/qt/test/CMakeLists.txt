# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# contains qt6-specific integration tests

add_executable(qt_ui_integration_test
    i18n_integration_test.cpp
)
target_link_libraries(qt_ui_integration_test PRIVATE qt_ui qt_ui_testing testing Qt6::Core Qt6::Test)
gtest_discover_tests(qt_ui_integration_test)

#
# ts to qm
#

set(ts_basename "test")
set(ts_filename "${ts_basename}.ts")
set(qm_filename "${ts_basename}.qm")
set(ts_file "${CMAKE_CURRENT_SOURCE_DIR}/${ts_filename}")
set(qm_file "${CMAKE_CURRENT_BINARY_DIR}/${qm_filename}")

# generate .qm using lrelease
add_custom_command(
    OUTPUT "${qm_file}"
    COMMAND Qt6::lrelease "${ts_file}" -qm "${qm_file}"
    DEPENDS "${ts_file}"
    VERBATIM
)

# add .qm file to qt resource at ":/i18n/test.qm"
set_source_files_properties("${qm_file}" PROPERTIES QT_RESOURCE_ALIAS "${qm_filename}")

# compile resource into test binary
qt6_add_resources(qt_ui_integration_test "test_translations"
    PREFIX "/i18n"
    FILES "${qm_file}"
)

# depend on custom command
target_sources(qt_ui_integration_test PRIVATE "${qm_file}")
