add_library(ui 
    model.hpp
    model.cpp
    presenter.hpp
    presenter.cpp
    renderer.hpp
)

# This library needs the headers and definitions from curves_lib
target_link_libraries(ui PUBLIC
    lib
)

# -----------------------------------------------------------------------------
# Project Subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(qt)

