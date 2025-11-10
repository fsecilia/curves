# -----------------------------------------------------------------------------
# Global Qt6 Settings
# -----------------------------------------------------------------------------

# Find qt packages.
find_package(Qt6 6.8.2 REQUIRED COMPONENTS
  Core
  Gui
  Qml
  Quick
  QuickControls2
)

# Automatically run moc, rcc, and uic.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------------------------------------------------------
# Main Executable
# -----------------------------------------------------------------------------

qt_add_executable(curves_config
  main.cpp
  resources.qrc 
)

set_target_properties(curves_config
  PROPERTIES
    OUTPUT_NAME curves
    VERSION ${PROJECT_VERSION}
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

# Link the executable against the Qt UI module and Qt libs
target_link_libraries(curves_config PRIVATE
  lib
  ui
  ui_qt
  Qt6::Quick
  Qt6::QuickControls2
)

# -----------------------------------------------------------------------------
# Qml Module
# -----------------------------------------------------------------------------
# The qml module is a plugin that bundles our C++ classes and makes them 
# available to qml.

qt_add_qml_module(ui_qt
  URI dev.mouse.curves
  VERSION 1.0    
  RESOURCE_PREFIX /
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dev/mouse/curves"
  SOURCES 
    view.hpp
    view.cpp
    painter_renderer.hpp
    painter_renderer.cpp
)

target_link_libraries(ui_qt PRIVATE
  lib
  ui
  Qt6::Quick
)
