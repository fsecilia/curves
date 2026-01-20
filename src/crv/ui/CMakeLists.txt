# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

add_executable(ui main.cpp)
target_link_libraries(ui PUBLIC driver_lib)

install(TARGETS ui RUNTIME
    DESTINATION bin
    COMPONENT ui
)
