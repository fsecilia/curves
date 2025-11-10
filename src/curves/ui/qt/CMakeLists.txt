# -----------------------------------------------------------------------------
# Global Qt6 Settings
# -----------------------------------------------------------------------------

# Tell Qt Creator's linter and designer where to find plugins
set(APP_PLUGIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugins")
list(PREPEND CMAKE_PREFIX_PATH "${APP_PLUGIN_DIR}")
set(ENV{QML_IMPORT_PATH} "${APP_PLUGIN_DIR}")
set_property(GLOBAL PROPERTY QT_QML_IMPORT_PATH "${APP_PLUGIN_DIR}")
set(QML_IMPORT_PATH "${APP_PLUGIN_DIR}"
    CACHE STRING "extra qml import paths"
    FORCE
)

# Automatically run moc, rcc, and uic.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find qt packages.
find_package(Qt6 6.8.2 REQUIRED COMPONENTS
  Core
  Gui
  Qml
  Quick
  QuickControls2
)

# -----------------------------------------------------------------------------
# Qml Module
# -----------------------------------------------------------------------------
# The qml module is a plugin that bundles our C++ classes and makes them
# available to qml.

qt_add_qml_module(ui_qt
  URI dev.mouse.curves
  VERSION 1.0
  SHARED
  DESIGNER_SUPPORTED
  RESOURCE_PREFIX /
  OUTPUT_DIRECTORY "${APP_PLUGIN_DIR}/dev/mouse/curves"
  SOURCES
    painter_renderer.hpp
    painter_renderer.cpp
    view.hpp
    view.cpp
)

target_link_libraries(ui_qt PRIVATE
  lib
  ui
  Qt6::Quick
)

set_target_properties(ui_qt PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${APP_PLUGIN_DIR}/dev/mouse/curves"
    RUNTIME_OUTPUT_DIRECTORY "${APP_PLUGIN_DIR}/dev/mouse/curves"
)

# -----------------------------------------------------------------------------
# Main Executable
# -----------------------------------------------------------------------------

qt_add_executable(curves_config
  CurveParameterModel.cpp
  CurveParameterModel.hpp
  CurveTypeModel.cpp
  CurveTypeModel.hpp
  EditorPresenter.cpp
  EditorPresenter.hpp
  main.cpp
  resources.qrc
)

set_target_properties(curves_config
  PROPERTIES
    OUTPUT_NAME curves
    QT_PLUGIN_PATH "${APP_PLUGIN_DIR}"
)

# Link the executable against the Qt UI module and Qt libs
target_link_libraries(curves_config PRIVATE
  lib
  ui
  ui_qt
  Qt6::Quick
  Qt6::QuickControls2
)
