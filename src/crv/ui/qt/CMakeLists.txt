# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# contains the qt6-specific ui

set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# source files in main executable
set(qt_files main.cpp)

# source files in library
set(qt_ui_files
    app.cpp
    app.hpp
    command/stack_adapter.cpp
    command/stack_adapter.hpp
    i18n.cpp
    i18n.hpp
    lib.hpp
    packed_curve.cpp
    packed_curve.hpp
    property_model.cpp
    property_model.hpp
    session_model.hpp
    session_view.cpp
    session_view.hpp
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

set(qml_cpp_files
    "${CMAKE_CURRENT_SOURCE_DIR}/widgets/graph/grid_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/widgets/graph/grid_renderer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/widgets/graph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/widgets/graph.hpp"
)

# let qt know not to expect repeated libraries on build command lines
set(QT_FORCE_CMP0156_TO_VALUE NEW)

# find qt
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    LinguistTools
    Qml
    QmlModels
    QmlWorkerScript
    Quick
    Test
    Widgets
)

# bail if linking against static libs
get_target_property(qt_qml_type Qt6::Qml TYPE)
if(NOT qt_qml_type STREQUAL "SHARED_LIBRARY")
    # if this pops, it's because you are trying to link MIT-licensed software against LGPL-licensed Qt
    message(FATAL_ERROR "A dynamically linked Qt installation is required. Found a static build instead.")
endif()

# expect resource prefix to be /qt/qml instead of /
qt_policy(SET QTP0001 NEW)

# enable qt's build automation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# qt ui lib
set(qt_ui_export_header ${CMAKE_CURRENT_BINARY_DIR}/crv/ui/qt/export.hpp)
add_library(qt_ui SHARED ${qt_ui_files})
generate_export_header(qt_ui
    EXPORT_MACRO_NAME qt-ui-api
    EXPORT_FILE_NAME ${qt_ui_export_header}
)
target_sources(qt_ui PRIVATE ${qt_ui_export_header})
target_include_directories(qt_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/widgets"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
set_target_properties(qt_ui PROPERTIES
    DISABLE_STRICT_ALIASING ON
    OUTPUT_NAME "crv-qt-ui"
)
target_link_libraries(qt_ui PUBLIC
    lib
    ui

    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QmlModels
    Qt6::QmlWorkerScript

    # link against widgets for message box until we have a way to report errors
    Qt6::Widgets
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(qt_ui PRIVATE
        -Wno-ctad-maybe-unsupported
        -Wno-extra-semi-stmt
    )
endif()

# register qml modules and attach them to target
qt_add_qml_module(qt_ui
    URI Curves
    VERSION 1.0
    QML_FILES ${qml_files}
    SOURCES ${qml_cpp_files}
    NO_PLUGIN
)

# main executable.
qt_add_executable(qt ${qt_files})
set_target_properties(qt PROPERTIES
    OUTPUT_NAME ${project_name_dashes}-config

    # System qml modules are distro runtime dependencies. Skip qt deployment/plugin scanning. Otherwise, trixie warns.
    QT_QML_MODULE_NO_IMPORT_SCAN ON
)
target_include_directories(qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/widgets")
target_link_libraries(qt PRIVATE qt_ui)

install(TARGETS qt RUNTIME
    DESTINATION bin
    COMPONENT ui
)

install(TARGETS qt_ui RUNTIME
    DESTINATION lib
    COMPONENT ui
)

qt6_add_translations(qt
    SOURCE_TARGETS
        qt
        qt_ui
    TS_FILES
        i18n/en.ts
    LUPDATE_OPTIONS "-tr-function-alias" "tr+=CRV_TR,translate+=CRV_TR_C"
    RESOURCE_PREFIX "/i18n"
)

if (crv_enable_testing)
    # common test library
    add_library(qt_ui_testing
        test_app.cpp
        test_app.hpp
    )
    target_link_libraries(qt_ui_testing PUBLIC qt_ui lib)

    # qt ui unit tests
    add_executable(qt_ui_test
        app_test.cpp
        command/stack_adapter_test.cpp
        i18n_test.cpp
        property_model_test.cpp
        session_model_test.cpp
        widgets/graph/grid_renderer_test.cpp
        widgets/graph_test.cpp
    )
    target_link_libraries(qt_ui_test PRIVATE qt_ui qt_ui_testing testing Qt6::Test)
    add_test(NAME qt_ui_test COMMAND qt_ui_test)
    add_dependencies(build_tests qt_ui_test)

    add_subdirectory(test)
endif()
