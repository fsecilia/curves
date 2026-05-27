# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia
#
# ui library

add_library(ui
    model/config.hpp
    serialization/exceptions.hpp
    serialization/toml/archive.hpp
    serialization/toml/archive.cpp
    serialization/toml/concepts.hpp
    serialization/toml/error_reporting.hpp
    serialization/toml/error_reporting.cpp
    serialization/toml/reader_adapter.hpp
    serialization/toml/writer_adapter.hpp
    serialization/reader.hpp
    serialization/serializer.hpp
    serialization/writer.hpp
    reflection/constraints.hpp
    reflection/enum.hpp
    reflection/param.hpp
)
target_link_libraries(ui PUBLIC lib tomlplusplus::tomlplusplus)

add_executable(ui_test
    model/config_test.cpp
    serialization/exceptions.cpp
    serialization/toml/archive_test.cpp
    serialization/toml/concepts_test.cpp
    serialization/toml/error_reporting_test.cpp
    serialization/toml/reader_adapter_test.cpp
    serialization/toml/writer_adapter_test.cpp
    serialization/reader_test.cpp
    serialization/serializer_test.cpp
    serialization/writer_test.cpp
    reflection/constraints_test.cpp
    reflection/enum_test.cpp
    reflection/param_test.cpp
)
target_link_libraries(ui_test PRIVATE ui lib testing)
gtest_discover_tests(ui_test)

add_subdirectory(qt)