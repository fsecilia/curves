// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "deserializer.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <sstream>

namespace crv::serialization {
namespace {

struct serialization_deserializer_test_t : Test
{
    struct document_t
    {
        int_t id = 0;
        constexpr auto operator==(document_t const&) const noexcept -> bool = default;
    };
    document_t read_document{5};

    struct reader_adapter_t
    {
        int_t id = 0;
        constexpr auto operator==(reader_adapter_t const&) const noexcept -> bool = default;
    };
    reader_adapter_t reader_adapter{7};

    struct reader_t
    {
        int_t id = 0;
        constexpr auto operator==(reader_t const&) const noexcept -> bool = default;
    };
    reader_t reader{11};

    struct mock_archive_t
    {
        virtual ~mock_archive_t() = default;

        MOCK_METHOD(reader_adapter_t, create_reader_adapter, (document_t document), (const));
        MOCK_METHOD(document_t, read_document_path, (std::filesystem::path const& path), (const));
        MOCK_METHOD(document_t, read_document_istream, (std::istream & out), (const));
    };
    StrictMock<mock_archive_t> mock_archive;

    struct archive_t
    {
        mock_archive_t* mock = nullptr;

        auto create_reader_adapter(document_t document) const -> reader_adapter_t
        {
            return mock->create_reader_adapter(document);
        }

        auto read_document(std::filesystem::path path) const -> document_t { return mock->read_document_path(path); }
        auto read_document(std::istream& in) const -> document_t { return mock->read_document_istream(in); }
    };

    struct mock_reader_factory_t
    {
        virtual ~mock_reader_factory_t() = default;

        MOCK_METHOD(reader_t, create_reader, (reader_adapter_t const& reader_adapter), (const));
    };
    StrictMock<mock_reader_factory_t> mock_reader_factory;

    struct reader_factory_t
    {
        mock_reader_factory_t* mock = nullptr;
        auto create_reader(reader_adapter_t const& reader_adapter) const -> reader_t
        {
            return mock->create_reader(reader_adapter);
        }
    };

    struct mock_reflected_t
    {
        virtual ~mock_reflected_t() = default;
        MOCK_METHOD(void, reflect, (reader_t reader), (const));
    };
    StrictMock<mock_reflected_t> mock_obj;

    struct reflected_t
    {
        mock_reflected_t* mock = nullptr;
        auto reflect(reader_t reader) const -> void { mock->reflect(reader); }
    };
    reflected_t obj{&mock_obj};

    using sut_t = deserializer_t<archive_t, reader_factory_t>;
    sut_t sut{archive_t{&mock_archive}, reader_factory_t{&mock_reader_factory}};
};

TEST_F(serialization_deserializer_test_t, serialize_to_path)
{
    auto const path = std::filesystem::path{"path"};

    EXPECT_CALL(mock_archive, read_document_path(path)).WillOnce(Return(read_document));
    EXPECT_CALL(mock_archive, create_reader_adapter(read_document)).WillOnce(Return(reader_adapter));
    EXPECT_CALL(mock_reader_factory, create_reader(reader_adapter)).WillOnce(Return(reader));
    EXPECT_CALL(mock_obj, reflect(reader));

    sut(path, obj);
}

TEST_F(serialization_deserializer_test_t, serialize_to_stream)
{
    auto in = std::istringstream{};

    EXPECT_CALL(mock_archive, read_document_istream(Ref(in))).WillOnce(Return(read_document));
    EXPECT_CALL(mock_archive, create_reader_adapter(read_document)).WillOnce(Return(reader_adapter));
    EXPECT_CALL(mock_reader_factory, create_reader(reader_adapter)).WillOnce(Return(reader));
    EXPECT_CALL(mock_obj, reflect(reader));

    sut(in, obj);
}

} // namespace
} // namespace crv::serialization
