# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui INTERFACE
)
target_link_libraries(ui INTERFACE lib)

# add_executable(ui_test PRIVATE
# )
# target_link_libraries(ui_test PRIVATE ui lib)