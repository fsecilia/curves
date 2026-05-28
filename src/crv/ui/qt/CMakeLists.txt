# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# contains the qt6-specific ui

# let qt know not to expect repeated libraries on build command lines
set(QT_FORCE_CMP0156_TO_VALUE NEW)

# find qt
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Widgets)

# enable qt's build automation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# main executable.
qt_add_executable(qt main.cpp)
set_target_properties(qt PROPERTIES OUTPUT_NAME ${project_name_dashes}-config)
target_include_directories(qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qt PRIVATE
    lib
    # ui

    Qt6::Gui
    Qt6::Qml
    Qt6::Quick

    # link against widgets for message box until we have a way to report errors
    Qt6::Widgets
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(qt PRIVATE
        -Wno-ctad-maybe-unsupported
        -Wno-unsafe-buffer-usage
        -Wno-unsafe-buffer-usage-in-libc-call
    )
endif()

install(TARGETS qt RUNTIME
    DESTINATION bin
    COMPONENT ui
)
