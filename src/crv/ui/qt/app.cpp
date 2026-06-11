// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "app.hpp"
#include <crv/serialization/exceptions.hpp>
#include <crv/serialization/toml/toml.hpp>
#include <QApplication>
#include <QStandardPaths>
#include <filesystem>
#include <format>
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

//
// app_t
//

app_t::app_t(QObject* parent) : QObject{parent}
{}

app_t::~app_t() = default;

auto app_t::initialize(int argc, char* argv[]) -> bool
{
    QGuiApplication::setApplicationName("curves");
    QGuiApplication::setOrganizationName("");

    auto const config_path = find_config_path();
    store_ = std::make_unique<config_store_t>(config_path);
    if (!load_model(argc, argv, *store_, model_root_)) return false;

    gui_app_ = std::make_unique<QGuiApplication>(argc, argv);
    engine_ = std::make_unique<QQmlApplicationEngine>();

    undo_stack_ = std::make_unique<command::stack_t<command::qt::stack_adapter_t<QUndoStack>>>(
        command::qt::stack_adapter_t{qt_undo_stack_});

    property_model_ = std::make_unique<property_model_t>(*undo_stack_, hierarchical_inspector_factory_t{});
    property_model_->load_config(model_root_);

    // ordered
    auto& context = *engine_->rootContext();
    context.setContextProperty("qtVersion", QT_VERSION);
    context.setContextProperty("undoStack", &qt_undo_stack_);
    context.setContextProperty("curvePropertyModel", property_model_.get());

    QObject::connect(
        engine_.get(), &QQmlApplicationEngine::objectCreationFailed, gui_app_.get(),
        []() { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

    engine_->loadFromModule("Curves", "Main");

    return true;
}

auto app_t::run() -> int
{
    return gui_app_->exec();
}

} // namespace crv
