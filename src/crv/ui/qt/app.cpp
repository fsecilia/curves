// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "app.hpp"
#include <crv/i18n.hpp>
#include <crv/model/config.hpp>
#include <crv/serialization/exceptions.hpp>
#include <crv/serialization/toml/toml.hpp>
#include <QStandardPaths>
#include <cstdio>
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

static auto report_error(QString message) -> void
{
    QMessageBox{QMessageBox::Critical, QString::fromStdString(CRV_TR("Curves Configuration Error")), std::move(message)}
        .exec();
}

static auto report_error(std::exception const& exception) -> void
{
    report_error(QString::fromStdString(CRV_TR("An unhandled exception occurred.\n\n{}", exception.what())));
}

static auto report_error(serialization::parse_x const& exception) -> void
{
    report_error(QString::fromStdString(CRV_TR("Could not parse config file.\n\n{}", exception.what())));
}

static auto report_error(serialization::io_x const& exception) -> void
{
    report_error(QString::fromStdString(CRV_TR("Could not write config file.\n\n{}", exception.what())));
}

static auto find_config_path() -> std::filesystem::path
{
    auto const config_root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return std::format("{}/config{}", config_root.toStdString(), serialization::tomlpp::archive_t::file_extension);
}

static auto load_model(config_store_t& store, model::root_t& root) noexcept -> bool
{
    try
    {
        root = store.load();
        store.save(root);
        return true;
    }
    catch (serialization::parse_x const& exception)
    {
        report_error(exception);
    }
    catch (serialization::io_x const& exception)
    {
        report_error(exception);
    }

    return false;
}

//
// app_t
//

app_t::app_t(int& argc, char** argv) : QApplication{argc, argv}
{
    setApplicationName(QString::fromStdString(CRV_TR(app_name)));
    setOrganizationName("");
}

app_t::~app_t() = default;

auto app_t::set_active_curve(int index) -> void
{
    if (index < 0 || index >= curve_names_.size()) return;

    auto const new_curve_id = static_cast<model::curves::curve_id_t>(index);

    // bail if curve is already active
    if (model_root_.profile.curves.active.value() == new_curve_id) return;

    // update backing model
    auto& active_curve = model_root_.profile.curves.active;
    using active_curve_t = std::remove_cvref_t<decltype(active_curve)>;
    command_stack_.template emplace_now<command::mutate_param_t<active_curve_t>>(false, active_curve, new_curve_id,
        [=, this](active_curve_t& command_param, active_curve_t::value_t const& cur) {
            if (cur == command_param.value()) return;
            load_active_curve_model();
        });
}

auto app_t::initialize() -> bool
{
    if (!translator_.load(QLocale(), "", "", ":/i18n")) return false;
    QCoreApplication::installTranslator(&translator_);
    i18n::provider(provider_);

    auto const config_path = find_config_path();
    store_ = std::make_unique<config_store_t>(config_path);
    if (!load_model(*store_, model_root_)) return false;

    for (auto curve_id = 0; curve_id < model::curves::curves_count; ++curve_id)
    {
        curve_names_.append(QString::fromStdString(
            CRV_TR(reflection::to_string(static_cast<model::curves::curve_id_t>(curve_id))->data())));
    }

    engine_ = std::make_unique<QQmlApplicationEngine>();

    command_stack_.observer(&command_stack_adapter_);

    device_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    device_model_->load_config(model_root_.device);

    profile_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    profile_model_->load_config(model_root_.profile, [](std::string_view nested_path) {
        // stop inspector from diving into curves section
        return nested_path != "curves";
    });

    scale_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    offset_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    baseline_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    limit_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    specific_curve_model_ = std::make_unique<property_model_t>(command_stack_, hierarchical_inspector_factory_t{});
    load_active_curve_model();

    // ordered
    auto& context = *engine_->rootContext();
    context.setContextProperty("qtVersion", QT_VERSION);
    context.setContextProperty("availableCurves", curve_names_);
    context.setContextProperty("undoStack", &command_stack_adapter_);
    context.setContextProperty("deviceModel", device_model_.get());
    context.setContextProperty("profileModel", profile_model_.get());
    context.setContextProperty("scaleModel", scale_model_.get());
    context.setContextProperty("offsetModel", offset_model_.get());
    context.setContextProperty("baselineModel", baseline_model_.get());
    context.setContextProperty("limitModel", limit_model_.get());
    context.setContextProperty("specificCurveModel", specific_curve_model_.get());
    context.setContextProperty("app", this);

    QObject::connect(
        engine_.get(), &QQmlApplicationEngine::objectCreationFailed, this,
        []() { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

    engine_->loadFromModule("Curves", "Main");

    baseline_model_->error_message(
        "height", QString::fromStdString(CRV_TR("baseline error message\nmore error message")));
    device_model_->error_message("dpi", QString::fromStdString(CRV_TR("dpi error message\nmore error message")));

    return true;
}

auto app_t::notify(QObject* receiver, QEvent* event) -> bool
{
    try
    {
        return QGuiApplication::notify(receiver, event);
    }
    catch (std::bad_alloc const&)
    {
        // Error reporting needs a dedicated, preallocated, hidden message box for oom eventually. For now, just print
        // to stderr with as little buffering as possible and abort.
        std::fputs(CRV_TR("Out of memory. Aborting!\n").data(), stderr);
    }
    catch (std::exception const& exception)
    {
        report_error(exception);
    }
    std::abort();
}

auto app_t::load_active_curve_model() -> void
{
    auto const target = static_cast<std::size_t>(model_root_.profile.curves.active.value());
    tuple::enumerate(model_root_.profile.curves.configs, [&](auto index, auto& curve_config) {
        if (index.value != target) return;

        scale_model_->load_config(curve_config.common.scale);
        offset_model_->load_config(curve_config.common.offset);
        baseline_model_->load_config(curve_config.common.baseline);
        limit_model_->load_config(curve_config.common.limit);
        specific_curve_model_->load_config(curve_config.specific);
    });

    emit activeCurveChanged();
}

} // namespace crv
