// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "serializer.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <sstream>

namespace crv::serialization {
namespace {

struct serialization_serializer_test_t : Test
{
    struct document_t
    {
        int_t id = 0;
        constexpr auto operator==(document_t const&) const noexcept -> bool = default;
    };
    document_t empty_document{5};

    struct writer_adapter_t
    {
        int_t id = 0;
        constexpr auto operator==(writer_adapter_t const&) const noexcept -> bool = default;
    };
    writer_adapter_t writer_adapter{7};

    struct writer_t
    {
        int_t id = 0;
        constexpr auto operator==(writer_t const&) const noexcept -> bool = default;
    };
    writer_t writer{11};

    struct mock_archive_t
    {
        virtual ~mock_archive_t() = default;

        MOCK_METHOD(document_t, create_empty_document, (), (const));
        MOCK_METHOD(writer_adapter_t, create_writer_adapter, (document_t document), (const));
        MOCK_METHOD(void, write_document_path, (document_t document, std::filesystem::path const& path), (const));
        MOCK_METHOD(void, write_document_ostream, (document_t document, std::ostream& out), (const));
    };
    StrictMock<mock_archive_t> mock_archive;

    struct archive_t
    {
        mock_archive_t* mock = nullptr;

        auto create_empty_document() const -> document_t { return mock->create_empty_document(); }
        auto create_writer_adapter(document_t document) const -> writer_adapter_t
        {
            return mock->create_writer_adapter(document);
        }

        auto write_document(document_t document, std::filesystem::path path) const -> void
        {
            mock->write_document_path(document, path);
        }

        auto write_document(document_t document, std::ostream& out) const -> void
        {
            mock->write_document_ostream(document, out);
        }
    };

    struct mock_writer_factory_t
    {
        virtual ~mock_writer_factory_t() = default;

        MOCK_METHOD(writer_t, create_writer, (writer_adapter_t const& writer_adapter), (const));
    };
    StrictMock<mock_writer_factory_t> mock_writer_factory;

    struct writer_factory_t
    {
        mock_writer_factory_t* mock = nullptr;
        auto create_writer(writer_adapter_t const& writer_adapter) const -> writer_t
        {
            return mock->create_writer(writer_adapter);
        }
    };

    struct mock_reflected_t
    {
        virtual ~mock_reflected_t() = default;
        MOCK_METHOD(void, reflect, (writer_t writer), (const));
    };
    StrictMock<mock_reflected_t> mock_obj;

    struct reflected_t
    {
        mock_reflected_t* mock = nullptr;
        auto reflect(writer_t writer) const -> void { mock->reflect(writer); }
    };
    reflected_t obj{&mock_obj};

    using sut_t = serializer_t<archive_t, writer_factory_t>;
    sut_t sut{archive_t{&mock_archive}, writer_factory_t{&mock_writer_factory}};
};

TEST_F(serialization_serializer_test_t, serialize_to_path)
{
    auto const path = std::filesystem::path{"path"};

    EXPECT_CALL(mock_archive, create_empty_document()).WillOnce(Return(empty_document));
    EXPECT_CALL(mock_archive, create_writer_adapter(empty_document)).WillOnce(Return(writer_adapter));
    EXPECT_CALL(mock_writer_factory, create_writer(writer_adapter)).WillOnce(Return(writer));
    EXPECT_CALL(mock_obj, reflect(writer));
    EXPECT_CALL(mock_archive, write_document_path(empty_document, path));

    sut(obj, path);
}

TEST_F(serialization_serializer_test_t, serialize_to_stream)
{
    auto out = std::ostringstream{};

    EXPECT_CALL(mock_archive, create_empty_document()).WillOnce(Return(empty_document));
    EXPECT_CALL(mock_archive, create_writer_adapter(empty_document)).WillOnce(Return(writer_adapter));
    EXPECT_CALL(mock_writer_factory, create_writer(writer_adapter)).WillOnce(Return(writer));
    EXPECT_CALL(mock_obj, reflect(writer));
    EXPECT_CALL(mock_archive, write_document_ostream(empty_document, Ref(out)));

    sut(obj, out);
}

} // namespace
} // namespace crv::serialization
