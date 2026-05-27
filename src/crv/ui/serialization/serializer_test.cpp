// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "serializer.hpp"
#include <crv/test/test.hpp>
#include <sstream>

namespace crv::serialization {
namespace {

struct serialization_serializer_test_t : Test
{
    using document_t = int_t;
    using writer_adapter_t = int_t;
    using writer_t = int_t;

    static constexpr auto empty_document = document_t{5};
    static constexpr auto writer_adapter = writer_adapter_t{empty_document + 7};
    static constexpr auto writer = writer_t{writer_adapter + 11};

    struct archive_results_t
    {
        document_t written_document = 0;
        std::filesystem::path actual_path = {};
        std::ostream* actual_ostream = nullptr;
    };
    archive_results_t archive_results{};

    struct archive_t
    {
        archive_results_t* results = nullptr;

        auto create_empty_document() const -> document_t { return empty_document; }
        auto create_writer_adapter(document_t document) const -> writer_adapter_t { return document + 7; }

        auto write_document(document_t document, std::filesystem::path path) const -> void
        {
            results->written_document = document;
            results->actual_path = std::move(path);
        }

        auto write_document(document_t document, std::ostream& out) const -> void
        {
            results->written_document = document;
            results->actual_ostream = &out;
        }
    };

    struct writer_factory_t
    {
        auto create_writer(writer_adapter_t writer_adapter) const -> writer_t { return writer_adapter + 11; }
    };

    struct reflected_t
    {
        mutable writer_t actual_writer = 0;
        auto reflect(writer_t writer) const -> void { actual_writer = writer; }
    };
    reflected_t obj;

    using sut_t = serializer_t<archive_t, writer_factory_t>;
    sut_t sut{archive_t{&archive_results}, writer_factory_t{}};
};

TEST_F(serialization_serializer_test_t, serialize_to_path)
{
    auto path = std::filesystem::path{"path"};
    sut(obj, path);

    EXPECT_EQ(writer, obj.actual_writer);
    EXPECT_EQ(empty_document, archive_results.written_document);
    EXPECT_EQ(path, archive_results.actual_path);
}

TEST_F(serialization_serializer_test_t, serialize_to_stream)
{
    auto out = std::ostringstream{};
    sut(obj, out);

    EXPECT_EQ(writer, obj.actual_writer);
    EXPECT_EQ(empty_document, archive_results.written_document);
    EXPECT_EQ(&out, archive_results.actual_ostream);
}

} // namespace
} // namespace crv::serialization
