// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "session_view.hpp"
#include <crv/i18n.hpp>
#include <crv/overloaded.hpp>
#include <crv/pipeline/configuration/construction/authored_validator.hpp>
#include <crv/pipeline/configuration/construction/gain_compiler.hpp>
#include <crv/pipeline/validator.hpp>
#include <crv/spline/construction/spline/amr/generation_result.hpp>
#include <cassert>
#include <format>
#include <system_error>
#include <utility>
#include <variant>

namespace crv::qt {
namespace {

using compiler_error_t = pipeline::configuration::compiler_t::error_t;
using authored_error_t = std::variant_alternative_t<0, compiler_error_t>;
using gain_error_t = std::variant_alternative_t<1, compiler_error_t>;
using runtime_error_t = std::variant_alternative_t<2, compiler_error_t>;
using sensitivity_error_t = std::variant_alternative_t<0, gain_error_t::detail_t>;
using spline_error_t = std::variant_alternative_t<1, gain_error_t::detail_t>;

static auto authored_error_message(authored_error_t const& result) -> QString
{
    using error_t = pipeline::configuration::construction::authored_validation_error_t;
    switch (result.error)
    {
        case error_t::none: break;
        case error_t::dpi: return QString::fromStdString(CRV_TR("Device DPI must be greater than zero."));
        case error_t::output_dpi:
            return QString::fromStdString(CRV_TR("Output DPI is outside the supported range for this device DPI."));
        case error_t::rotation:
            return QString::fromStdString(CRV_TR("Device rotation is outside the supported range."));
        case error_t::anisotropy:
            return QString::fromStdString(CRV_TR("Profile anisotropy is outside the supported range."));
        case error_t::filter_half_life:
            return QString::fromStdString(CRV_TR("Filter half-life is outside the supported range."));
        case error_t::filter_half_life_underflow:
            return QString::fromStdString(CRV_TR("Filter half-life is too small to represent at runtime."));
        case error_t::curve_id: return QString::fromStdString(CRV_TR("The selected curve is invalid."));
        case error_t::unsupported_shaping:
            return QString::fromStdString(CRV_TR("The selected shaping configuration is not supported."));
        case error_t::synchronous_motivity:
            return QString::fromStdString(CRV_TR("Synchronous motivity is outside the supported range."));
        case error_t::synchronous_gamma:
            return QString::fromStdString(CRV_TR("Synchronous gamma is outside the supported range."));
        case error_t::synchronous_smooth:
            return QString::fromStdString(CRV_TR("Synchronous smoothing is outside the supported range."));
        case error_t::synchronous_sync_speed:
            return QString::fromStdString(CRV_TR("Synchronous sync speed is outside the supported range."));
        case error_t::log_normal_baseline:
            return QString::fromStdString(CRV_TR("Log-normal baseline gain is outside the supported range."));
        case error_t::log_normal_limit:
            return QString::fromStdString(CRV_TR("Log-normal limit gain is outside the supported range."));
        case error_t::log_normal_accel_peak:
            return QString::fromStdString(CRV_TR("Log-normal acceleration peak is outside the supported range."));
        case error_t::log_normal_max_accel:
            return QString::fromStdString(CRV_TR("Log-normal maximum acceleration is outside the supported range."));
        case error_t::log_normal_parameters:
            return QString::fromStdString(CRV_TR("The log-normal parameters do not define a valid curve."));
    }

    assert(false && "unexpected successful authored validation result in compiler error");
    return QString::fromStdString(CRV_TR("Authored configuration validation failed."));
}

static auto gain_error_message(gain_error_t const& error) -> QString
{
    return std::visit(overloaded_t{
                          [](sensitivity_error_t const& detail) {
                              return QString::fromStdString(CRV_TR(
                                  "Could not refine the sensitivity curve accurately enough (error {}, limit {}).",
                                  detail.achieved_error, detail.max_error));
                          },
                          [](spline_error_t const& detail) {
                              using reason_t = spline::spline_generation_error_reason_t;
                              switch (detail.reason)
                              {
                                  case reason_t::segment_budget_exhausted:
                                      return QString::fromStdString(
                                          CRV_TR("Could not build the acceleration curve: segment budget exhausted."));
                                  case reason_t::minimum_interval_width:
                                      return QString::fromStdString(CRV_TR(
                                          "Could not build the acceleration curve: minimum interval width reached."));
                              }
                              assert(false && "unexpected spline generation failure");
                              return QString::fromStdString(CRV_TR("Could not build the acceleration curve."));
                          },
                      },
        error.detail);
}

static auto runtime_error_message(runtime_error_t const& result) -> QString
{
    using error_t = pipeline::runtime_config_validation_error_t;
    switch (result.error)
    {
        case error_t::none: break;
        case error_t::velocity_scale:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the derived velocity scale."));
        case error_t::output_scale:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the derived output scale."));
        case error_t::half_life:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the derived filter half-life."));
        case error_t::output_transform_rotation_component:
            return QString::fromStdString(CRV_TR("Runtime validation rejected an output rotation component."));
        case error_t::output_transform_anisotropy_component:
            return QString::fromStdString(CRV_TR("Runtime validation rejected an output anisotropy component."));
        case error_t::output_transform_rotation_norm:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the output rotation norm."));
        case error_t::output_transform_anisotropy_norm:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the output anisotropy norm."));
        case error_t::output_transform_orthogonality:
            return QString::fromStdString(CRV_TR("Runtime validation rejected output-transform orthogonality."));
        case error_t::output_transform_determinant:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the output-transform determinant."));
        case error_t::spline_locator:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the acceleration-curve locator."));
        case error_t::spline_segment:
            return QString::fromStdString(
                CRV_TR("Runtime validation rejected acceleration-curve segment {}.", result.segment_index));
        case error_t::spline_tangent:
            return QString::fromStdString(CRV_TR("Runtime validation rejected the acceleration-curve tangent."));
        case error_t::spline_tangent_anchor:
            return QString::fromStdString(
                CRV_TR("Runtime validation rejected tangent anchor segment {}.", result.segment_index));
    }

    assert(false && "unexpected successful runtime validation result in compiler error");
    return QString::fromStdString(CRV_TR("Runtime configuration validation failed."));
}

static auto compiler_error_message(compiler_error_t const& error) -> QString
{
    return std::visit(overloaded_t{
                          [](authored_error_t const& detail) { return authored_error_message(detail); },
                          [](gain_error_t const& detail) { return gain_error_message(detail); },
                          [](runtime_error_t const& detail) { return runtime_error_message(detail); },
                      },
        error);
}

} // namespace

session_view_t::session_view_t(session_model_t model) noexcept : model_{std::move(model)} {}

auto session_view_t::open() -> open_result_t
{
    auto session = pipeline::control::session_t::open();
    if (!session) return std::unexpected{std::move(session.error())};

    auto result = std::unique_ptr<session_view_t>{new session_view_t{session_model_t{std::move(*session)}}};
    result->refresh();
    return result;
}

auto session_view_t::control_error_message(pipeline::control::error_t const& error) -> QString
{
    using error_t = pipeline::control::error_code_t;
    auto message = QString{};
    switch (error.code)
    {
        case error_t::endpoint_open_failed:
            message = QString::fromStdString(CRV_TR("Could not open the Curves control endpoint."));
            break;
        case error_t::enumeration_failed:
            message = QString::fromStdString(CRV_TR("Could not enumerate live Curves devices."));
            break;
        case error_t::attachment_unavailable:
            message = QString::fromStdString(CRV_TR("The selected live device is no longer available."));
            break;
        case error_t::apply_rejected:
            message = QString::fromStdString(CRV_TR("The kernel rejected the runtime configuration."));
            break;
        case error_t::apply_failed:
            message = QString::fromStdString(CRV_TR("Could not apply the runtime configuration."));
            break;
    }

    if (error.native_error != 0)
    {
        auto const native_message
            = std::error_code{static_cast<int>(error.native_error), std::generic_category()}.message();
        message += QString::fromStdString(CRV_TR("\nerrno {}: {}", error.native_error, native_message));
    }
    return message;
}

auto session_view_t::attachment_labels() const -> QStringList
{
    auto result = QStringList{};
    result.reserve(static_cast<qsizetype>(model_.attachments().size()));
    for (auto const& attachment : model_.attachments())
    {
        result.append(QString::fromStdString(
            std::format("{} — {:04x}:{:04x}", attachment.sysname, attachment.vendor, attachment.product)));
    }
    return result;
}

auto session_view_t::selected_attachment_index() const noexcept -> int
{
    return static_cast<int>(model_.selected_attachment_index());
}

auto session_view_t::can_apply() const noexcept -> bool
{
    return model_.has_selected_attachment();
}

auto session_view_t::refresh_model() -> std::optional<pipeline::control::error_t>
{
    auto const result = model_.refresh();
    if (!result) return result.error();

    emit attachmentsChanged();
    emit selectedAttachmentChanged();
    return std::nullopt;
}

auto session_view_t::refresh() -> void
{
    if (auto const error = refresh_model())
    {
        set_status(control_error_message(*error), true);
        return;
    }

    if (model_.attachments().empty())
    {
        set_status(QString::fromStdString(CRV_TR("No live Curves devices are attached.")), false);
        return;
    }
    if (!model_.has_selected_attachment())
    {
        set_status(QString::fromStdString(CRV_TR("Select a live device to enable Apply and Disable.")), false);
        return;
    }
    set_status({}, false);
}

auto session_view_t::selectAttachment(int index) -> void
{
    auto const previous = model_.selected_attachment_index();
    model_.set_selected_attachment_index(static_cast<int_t>(index));
    if (previous != model_.selected_attachment_index()) emit selectedAttachmentChanged();
    if (model_.has_selected_attachment() && !status_is_error_) set_status({}, false);
}

auto session_view_t::apply_active(model::device_t const& device, model::profile_t const& profile) -> bool
{
    return apply(device, profile, pipeline::configuration::apply_mode_t::active);
}

auto session_view_t::disable(model::device_t const& device, model::profile_t const& profile) -> bool
{
    auto const result = apply(device, profile, pipeline::configuration::apply_mode_t::bypassed);
    if (result) set_status(QString::fromStdString(CRV_TR("Acceleration disabled for the selected device.")), false);
    return result;
}

auto session_view_t::apply(model::device_t const& device, model::profile_t const& profile,
    pipeline::configuration::apply_mode_t mode) -> bool
{
    if (!model_.has_selected_attachment())
    {
        set_status(QString::fromStdString(CRV_TR("Select a live device before applying configuration.")), true);
        return false;
    }

    auto const result = model_.apply(device, profile, mode);
    if (result)
    {
        set_status({}, false);
        return true;
    }

    present_apply_error(result.error());
    return false;
}

auto session_view_t::present_apply_error(session_model_t::apply_result_t::error_type const& error) -> void
{
    std::visit(overloaded_t{
                   [&](compiler_error_t const& detail) { set_status(compiler_error_message(detail), true); },
                   [&](pipeline::control::error_t const& detail) {
                       auto message = control_error_message(detail);
                       if (detail.code == pipeline::control::error_code_t::attachment_unavailable)
                       {
                           auto const previous_selection = model_.selected_attachment_index();
                           model_.clear_selected_attachment();
                           if (previous_selection != model_.selected_attachment_index())
                               emit selectedAttachmentChanged();

                           if (auto const refresh_error = refresh_model())
                           {
                               message += QString::fromStdString(
                                   CRV_TR("\n\nRefreshing the live device list also failed:\n"));
                               message += control_error_message(*refresh_error);
                           }
                       }
                       set_status(std::move(message), true);
                   },
               },
        error);
}

auto session_view_t::report_saved() -> void
{
    set_status(QString::fromStdString(CRV_TR("Configuration applied and saved.")), false);
}

auto session_view_t::report_save_failure(QString const& detail) -> void
{
    auto message = QString::fromStdString(
        CRV_TR("Live configuration was applied, but saving the authored settings failed."));
    if (!detail.isEmpty()) message += "\n" + detail;
    set_status(std::move(message), true);
}

auto session_view_t::set_status(QString text, bool is_error) -> void
{
    if (status_text_ == text && status_is_error_ == is_error) return;
    status_text_ = std::move(text);
    status_is_error_ = is_error;
    emit statusChanged();
}

} // namespace crv::qt
