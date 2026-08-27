// SPDX-License-Identifier: MIT

/// \file
/// \brief Qt presentation for a live control session
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/pipeline/control/client.hpp>
#include <crv/ui/qt/lib.hpp>
#include <crv/ui/qt/session_model.hpp>
#include <QObject>
#include <QString>
#include <QStringList>
#include <expected>
#include <memory>
#include <optional>

namespace crv::qt {

class qt_ui_api session_view_t : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList attachmentLabels READ attachment_labels NOTIFY attachmentsChanged)
    Q_PROPERTY(int selectedAttachmentIndex READ selected_attachment_index NOTIFY selectedAttachmentChanged)
    Q_PROPERTY(bool canApply READ can_apply NOTIFY selectedAttachmentChanged)
    Q_PROPERTY(QString statusText READ status_text NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ status_is_error NOTIFY statusChanged)

public:
    using open_result_t = std::expected<std::unique_ptr<session_view_t>, pipeline::control::error_t>;

    [[nodiscard]] static auto open() -> open_result_t;
    static auto control_error_message(pipeline::control::error_t const& error) -> QString;

    auto attachment_labels() const -> QStringList;
    auto selected_attachment_index() const noexcept -> int;
    auto can_apply() const noexcept -> bool;
    auto status_text() const -> QString const& { return status_text_; }
    auto status_is_error() const noexcept -> bool { return status_is_error_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectAttachment(int index);

    auto apply_active(model::device_t const& device, model::profile_t const& profile) -> bool;
    auto disable(model::device_t const& device, model::profile_t const& profile) -> bool;
    auto report_saved() -> void;
    auto report_save_failure(QString const& detail) -> void;

signals:
    void attachmentsChanged();
    void selectedAttachmentChanged();
    void statusChanged();

private:
    explicit session_view_t(session_model_t model) noexcept;

    auto apply(model::device_t const& device, model::profile_t const& profile,
        pipeline::configuration::apply_mode_t mode) -> bool;
    auto refresh_model() -> std::optional<pipeline::control::error_t>;
    auto present_apply_error(session_model_t::apply_result_t::error_type const& error) -> void;
    auto set_status(QString text, bool is_error) -> void;

    session_model_t model_;
    QString status_text_;
    bool status_is_error_ = false;
};

} // namespace crv::qt
