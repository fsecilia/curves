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
#include <cstdlib>
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
{}

app_t::~app_t() = default;

auto app_t::construct(int& argc, char** argv) -> std::unique_ptr<app_t>
{
    setApplicationName(QString::fromStdString(CRV_TR(app_name)));
    setOrganizationName("");

    auto result = std::unique_ptr<app_t>{new app_t{argc, argv}};

    if (!result->translator_.load(QLocale(), "", "", ":/i18n"))
    {
        result.reset();
        return result;
    }

    QCoreApplication::installTranslator(&result->translator_);
    i18n::provider(result->provider_);

    auto const config_path = find_config_path();
    result->store_ = std::make_unique<config_store_t>(config_path);
    if (!load_model(*result->store_, result->model_root_))
    {
        result.reset();
        return result;
    }

    auto session_view = qt::session_view_t::open();
    if (!session_view)
    {
        report_error(qt::session_view_t::control_error_message(session_view.error()));
        result.reset();
        return result;
    }
    result->session_view_ = std::move(*session_view);

    for (auto curve_id = 0; curve_id < model::curves::curves_count; ++curve_id)
    {
        result->curve_names_.append(QString::fromStdString(
            CRV_TR(reflection::to_string(static_cast<model::curves::curve_id_t>(curve_id))->data())));
    }

    result->engine_ = std::make_unique<QQmlApplicationEngine>();

    result->command_stack_.observer(&result->command_stack_adapter_);

    result->device_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->device_model_->load_config(result->model_root_.device);
    result->update_dpi_state();

    result->profile_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->profile_model_->load_config(result->model_root_.profile, [](std::string_view nested_path) {
        // stop inspector from diving into curves section
        return nested_path != "curves";
    });

    result->scale_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->offset_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->anchor_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->ceiling_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->specific_curve_model_
        = std::make_unique<property_model_t>(result->command_stack_, hierarchical_inspector_factory_t{});
    result->load_active_curve_model();

    QObject::connect(
        result->device_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);
    QObject::connect(
        result->profile_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);
    QObject::connect(
        result->scale_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);
    QObject::connect(
        result->offset_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);
    QObject::connect(
        result->anchor_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);
    QObject::connect(
        result->ceiling_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);
    QObject::connect(
        result->specific_curve_model_.get(), &property_model_t::valueChanged, result.get(), &app_t::on_model_changed);

    // ordered
    auto& context = *result->engine_->rootContext();
    context.setContextProperty("qtVersion", QT_VERSION);
    context.setContextProperty("availableCurves", result->curve_names_);
    context.setContextProperty("undoStack", &result->command_stack_adapter_);
    context.setContextProperty("deviceModel", result->device_model_.get());
    context.setContextProperty("profileModel", result->profile_model_.get());
    context.setContextProperty("scaleModel", result->scale_model_.get());
    context.setContextProperty("offsetModel", result->offset_model_.get());
    context.setContextProperty("anchorModel", result->anchor_model_.get());
    context.setContextProperty("ceilingModel", result->ceiling_model_.get());
    context.setContextProperty("specificCurveModel", result->specific_curve_model_.get());
    context.setContextProperty("sessionView", result->session_view_.get());
    context.setContextProperty("app", result.get());

    QObject::connect(
        result->engine_.get(), &QQmlApplicationEngine::objectCreationFailed, result.get(),
        []() { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

    result->engine_->loadFromModule("Curves", "Main");

    return result;
}

auto app_t::apply() -> void
{
    if (!session_view_->apply_active(model_root_.device, model_root_.profile)) return;

    try
    {
        store_->save(model_root_);
        session_view_->report_saved();
    }
    catch (std::bad_alloc const&)
    {
        throw;
    }
    catch (std::exception const& exception)
    {
        session_view_->report_save_failure(QString::fromLocal8Bit(exception.what()));
    }
}

auto app_t::disable() -> void
{
    session_view_->disable(model_root_.device, model_root_.profile);
}

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
            emit activeCurveChanged();
            emit curveChanged(curve());
        });
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
    tuple::visit_at(model_root_.profile.curves.configs, target, [&](auto& curve_config) {
        scale_model_->load_config(curve_config.common.scale);
        offset_model_->load_config(curve_config.common.offset);
        anchor_model_->load_config(curve_config.common.anchor);
        ceiling_model_->load_config(curve_config.common.ceiling);
        specific_curve_model_->load_config(curve_config.specific);

        curve_ = model::curves::create_composed_curve<float_t>(curve_config.specific);
    });
}


auto app_t::update_dpi_state() -> void
{
    auto const configured = dpiConfigured();
    device_model_->error_message(
        "dpi", configured ? QString{} : QString::fromStdString(CRV_TR("Enter mouse DPI to begin.")));
    emit dpiConfiguredChanged();
}

auto app_t::on_model_changed(QString path, QVariant const&) -> void
{
    if (path == "dpi") update_dpi_state();

    auto const target = static_cast<std::size_t>(model_root_.profile.curves.active.value());
    tuple::visit_at(model_root_.profile.curves.configs, target,
        [&](auto& curve_config) { curve_ = model::curves::create_composed_curve<float_t>(curve_config.specific); });

    emit curveChanged(curve());
}

} // namespace crv
