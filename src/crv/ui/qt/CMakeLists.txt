# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# contains the qt6-specific ui

# source files in main executable
set(qt_files main.cpp)

# source files in library
set(qt_ui_files
    app.cpp
    app.hpp
    command/stack_adapter.hpp
    command/stack_adapter.cpp
    i18n.cpp
    i18n.hpp
    property_model.cpp
    property_model.hpp
)

# qml files
set(qml_files
    CurveConfig.qml
    CurveSelector.qml
    GraphView.qml
    FloatField.qml
    FloatField_v6_9.qml
    IntField.qml
    IntField_v6_9.qml
    Main.qml
    NumericField.qml
    PropertyDelegateChooser.qml
    RootConfigRow.qml
)

# let qt know not to expect repeated libraries on build command lines
set(QT_FORCE_CMP0156_TO_VALUE NEW)

# find qt
find_package(Qt6 REQUIRED COMPONENTS Gui LinguistTools Qml Quick Test Widgets)

# bail if linking against static libs
get_target_property(qt_qml_type Qt6::Qml TYPE)
if(NOT qt_qml_type STREQUAL "SHARED_LIBRARY")
    # if this pops, it's because you are trying to link MIT-licensed software against LGPL-licensed Qt
    message(FATAL_ERROR "A dynamically linked Qt installation is required. Found a static build instead.")
endif()

# expect reource prefix to be /qt/qml instead of /
qt_policy(SET QTP0001 NEW)

# enable qt's build automation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# qt ui lib
add_library(qt_ui ${qt_ui_files})
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

# register qml module and attach it to target
qt_add_qml_module(qml
    URI "Curves"
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Curves"
    QML_FILES ${qml_files}
)
target_compile_options(qml PRIVATE
    -fno-strict-aliasing
    -Wno-strict-aliasing
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(qml PRIVATE -Wno-extra-semi-stmt)
endif()

# common test library
add_library(qt_ui_testing
    test_app.cpp
    test_app.hpp
)
set_target_properties(qt_ui_testing PROPERTIES QT_EXCLUDE_FROM_TRANSLATION ON)
target_link_libraries(qt_ui_testing PUBLIC qt_ui lib)

# qt ui unit tests
add_executable(qt_ui_test
    app_test.cpp
    command/stack_adapter_test.cpp
    i18n_test.cpp
    property_model_test.cpp
)
target_link_libraries(qt_ui_test PRIVATE qt_ui qt_ui_testing testing Qt6::Test)
gtest_discover_tests(qt_ui_test)

# main executable.
qt_add_executable(qt ${qt_files})
set_target_properties(qt
    PROPERTIES OUTPUT_NAME ${project_name_dashes}-config
    QT_EXCLUDE_FROM_TRANSLATION ON
)
target_include_directories(qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qt PRIVATE qt_ui qml)

install(TARGETS qt RUNTIME
    DESTINATION bin
    COMPONENT ui
)

# translations
qt6_add_translations(qt
    TS_FILES
        i18n/en.ts
    SOURCES
        ${qt_files}
        ${qt_ui_files}
        ${qml_files}
    LUPDATE_OPTIONS "-tr-function-alias" "tr+=CRV_TR,translate+=CRV_TR_C"
    RESOURCE_PREFIX "/i18n"
)

add_subdirectory(test)
