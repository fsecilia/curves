# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# contains the qt6-specific ui

# let qt know not to expect repeated libraries on build command lines
set(QT_FORCE_CMP0156_TO_VALUE NEW)

# find qt
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Widgets)

# expect reource prefix to be /qt/qml instead of /
qt_policy(SET QTP0001 NEW)

# enable qt's build automation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# qt ui lib
add_library(qt_ui
    command/command_adapter.hpp
    property_model.cpp
    property_model.hpp
)
target_link_libraries(qt_ui PUBLIC
    lib
    ui

    Qt6::Gui
    Qt6::Qml
    Qt6::Quick

    # link against widgets for message box until we have a way to report errors
    Qt6::Widgets
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(qt_ui PRIVATE
        -Wno-ctad-maybe-unsupported
        -Wno-unsafe-buffer-usage
        -Wno-unsafe-buffer-usage-in-libc-call
    )
endif()

# qt ui unit tests
add_executable(qt_ui_test
    command/command_adapter_test.cpp
    property_model_test.cpp
)
target_link_libraries(qt_ui_test PRIVATE qt_ui testing)
gtest_discover_tests(qt_ui_test)

# main executable.
qt_add_executable(qt main.cpp)
set_target_properties(qt PROPERTIES OUTPUT_NAME ${project_name_dashes}-config)
target_include_directories(qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qt PRIVATE qt_ui)

# register QML module and attach it to target
qt_add_qml_module(qt
    URI "Curves"
    VERSION 1.0
    QML_FILES Main.qml
)

# disable strict aliasing in module where qt depends on it
#
# This is quite fragile, but the alternative is adding a dedicated set of project options and adding that to every other
# project, something easily missed. Of the evils, this is lesser. If a new version of qt moves this file, this will
# break andd need to be revisited. It will fail loudly, though the cause may not be obvious.
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/qt_qmlcache_loader.cpp"
    PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-Wno-strict-aliasing"
)

install(TARGETS qt RUNTIME
    DESTINATION bin
    COMPONENT ui
)
