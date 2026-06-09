// SPDX-License-Identifier: MIT

/// \file
/// \brief config app qt entry point
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/model/config.hpp>
#include <crv/serialization/exceptions.hpp>
#include <crv/serialization/toml/toml.hpp>
#include <crv/ui/qt/property_model.hpp>
#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <utility>

namespace crv {

class config_store_t
{
public:
    using deserializer_t = serialization::tomlpp::deserializer_t;
    using serializer_t = serialization::tomlpp::serializer_t;

    explicit config_store_t(std::filesystem::path path) noexcept
        : config_store_t{std::move(path), deserializer_t{}, serializer_t{}}
    {}

    config_store_t(std::filesystem::path path, deserializer_t deserializer, serializer_t serializer) noexcept
        : path_{std::move(path)}, deserializer_{std::move(deserializer)}, serializer_{std::move(serializer)}
    {}

    [[nodiscard]] auto load() const -> model::root_t
    {
        auto root = model::root_t{};
        if (std::filesystem::exists(path_)) { deserializer_(path_, root); }
        return root;
    }

    auto save(model::root_t const& root) const -> void
    {
        std::filesystem::create_directories(path_.parent_path());
        serializer_(root, path_);
    }

private:
    std::filesystem::path path_;
    serialization::tomlpp::deserializer_t deserializer_;
    serialization::tomlpp::serializer_t serializer_;
};

static auto report_error(int argc, char* argv[], QString message) -> void
{
    QApplication::setApplicationName("curves");
    QApplication::setOrganizationName("");

    auto widgets_app = QApplication{argc, argv};

    QMessageBox{QMessageBox::Critical, "Curves Configuration Error", std::move(message)}.exec();
}

static auto report_error(int argc, char* argv[], serialization::parse_x const& exception) -> void
{
    auto const message = std::format("Could not parse config file.\n\n{}", exception.what());

    report_error(argc, argv, QString::fromStdString(message));
}

static auto report_error(int argc, char* argv[], serialization::io_x const& exception) -> void
{
    auto const message = std::format("Could not write config file.\n\n{}", exception.what());

    report_error(argc, argv, QString::fromStdString(message));
}

static auto find_config_path() -> std::filesystem::path
{
    auto const config_root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return std::format("{}/config{}", config_root.toStdString(), serialization::tomlpp::archive_t::file_extension);
}

static auto load_model(int argc, char* argv[], config_store_t& store, model::root_t& root) noexcept -> bool
{
    try
    {
        root = store.load();
        store.save(root);
        return true;
    }
    catch (serialization::parse_x const& exception)
    {
        report_error(argc, argv, exception);
    }
    catch (serialization::io_x const& exception)
    {
        report_error(argc, argv, exception);
    }

    return false;
}

static auto main(int argc, char* argv[]) -> int
{
    QGuiApplication::setApplicationName("curves");
    QGuiApplication::setOrganizationName("");

    auto const config_path = find_config_path();
    auto const store = std::make_unique<config_store_t>(config_path);
    auto model_root = model::root_t{};
    if (!load_model(argc, argv, *store, model_root)) return EXIT_FAILURE;

    auto gui_app = QGuiApplication{argc, argv};

    QQmlApplicationEngine engine;

    auto undo_stack = QUndoStack{};
    auto curve_property_model = curve_property_model_t{undo_stack, {}, {}};
    curve_property_model.load_config(model_root);
    engine.rootContext()->setContextProperty("curvePropertyModel", &curve_property_model);
    engine.rootContext()->setContextProperty("undoStack", &undo_stack);

    // exit if QML engine fails to load root object
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &gui_app, []() { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);

    // load QML file from module defined in CMake
    engine.loadFromModule("Curves", "Main");

    return gui_app.exec();
}

} // namespace crv

auto main(int argc, char* argv[]) -> int
{
    return crv::main(argc, argv);
}
