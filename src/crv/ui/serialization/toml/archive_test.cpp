// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "archive.hpp"
#include <crv/test/test.hpp>

namespace crv::serialization::tomlpp {
namespace {

struct serialization_tomlpp_archive_test_t : Test
{
    archive_t archive;
};

TEST_F(serialization_tomlpp_archive_test_t, has_correct_extension)
{
    EXPECT_EQ(archive_t::file_extension, ".toml");
}

TEST_F(serialization_tomlpp_archive_test_t, created_empty_document_is_empty)
{
    auto document = archive.create_empty_document();
    EXPECT_TRUE(document.empty());
}

TEST_F(serialization_tomlpp_archive_test_t, stream_round_trip)
{
    auto out_document = archive.create_empty_document();
    out_document.insert("string", "value");
    out_document.insert("int", 7);

    auto stream = std::stringstream{};
    archive.write_document(out_document, stream);

    auto const in_document = archive.read_document(stream);
    EXPECT_STREQ(in_document["string"].value_or(""), "value");
    EXPECT_EQ(in_document["int"].value_or(0), 7);
}

// --------------------------------------------------------------------------------------------------------------------

struct serialization_tomlpp_archive_test_against_file_t : serialization_tomlpp_archive_test_t
{
    void SetUp() override { file_path = std::filesystem::temp_directory_path() / "archive_test_file.toml"; }

    void TearDown() override
    {
        if (std::filesystem::exists(file_path)) std::filesystem::remove(file_path);
    }

    std::filesystem::path file_path;
};

TEST_F(serialization_tomlpp_archive_test_against_file_t, file_round_trip)
{
    auto out_document = archive.create_empty_document();
    out_document.insert("bool", true);
    out_document.insert("float", 8.0);

    archive.write_document(out_document, file_path);
    ASSERT_TRUE(std::filesystem::exists(file_path));

    auto const in_document = archive.read_document(file_path);
    EXPECT_TRUE(in_document["bool"].value_or(false));
    EXPECT_EQ(in_document["float"].value_or(0.0), 8.0);
}

TEST_F(serialization_tomlpp_archive_test_against_file_t, write_to_invalid_path_throws_io_x)
{
    auto document = archive.create_empty_document();
    document.insert("key", "value");

    // directory path cannot be opened as a regular file for writing
    std::filesystem::path invalid_path = std::filesystem::temp_directory_path();

    EXPECT_THROW(archive.write_document(document, invalid_path), io_x);
}

TEST_F(serialization_tomlpp_archive_test_against_file_t, write_to_bad_stream_throws_io_x)
{
    auto document = archive.create_empty_document();
    document.insert("key", "value");

    auto closed_stream = std::ofstream{};

    // intentionally leave stream closed and force failure state
    closed_stream.setstate(std::ios::failbit);

    EXPECT_THROW(archive.write_document(document, closed_stream), io_x);
}

} // namespace
} // namespace crv::serialization::tomlpp
