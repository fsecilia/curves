# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

add_executable(ui main.cpp)
target_link_libraries(ui PUBLIC driver_lib)
set_target_properties(ui PROPERTIES OUTPUT_NAME curves-mouse-acceleration-ui)
install(TARGETS ui RUNTIME
    DESTINATION bin
    COMPONENT ui
)
