// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/float_limits.hpp>
#include <crv/math/limits.hpp>
#include <crv/reflection/enum.hpp>
#include <crv/ui/command/mutate_param.hpp>
#include <crv/ui/command/stack.hpp>
#include <crv/ui/hierarchical_inspector.hpp>
#include <crv/ui/qt/command/stack_adapter.hpp>
#include <QAbstractListModel>
#include <QString>
#include <QUndoStack>
#include <QVariant>
#include <functional>
#include <optional>
#include <vector>

namespace crv {

/// maps to QML DelegateChooser choices
enum class property_type_id_t
{
    float_t,
    int_t,
    bool_t
};

/// type-erased representation of a reflected parameter
struct ui_node_t
{
    QString path;
    property_type_id_t type;
    QVariant value;
    QVariant min;
    QVariant max;

    // executes the command logic, passing the new value from QML
    std::function<auto(QVariant const&, bool)->void> push_command;
};

class property_model_t : public QAbstractListModel
{
    Q_OBJECT

public:
    enum class roles_t : int
    {
        path = Qt::UserRole + 1,
        type,
        value,
        min,
        max
    };

    using undo_stack_t = command::observable_stack_t<command::stack_t<>>;

    explicit property_model_t(undo_stack_t& undo_stack, hierarchical_inspector_factory_t hierarchical_inspector_factory,
        QObject* parent = nullptr);

    auto rowCount(QModelIndex const& parent = QModelIndex()) const -> int override;
    auto data(QModelIndex const& index, int role = Qt::DisplayRole) const -> QVariant override;
    auto setData(QModelIndex const& index, QVariant const& value, int role = Qt::EditRole) -> bool override;
    auto roleNames() const -> QHash<int, QByteArray> override;

    // called back from qml to handle changes from mouse wheel
    Q_INVOKABLE auto on_wheel(int row, QVariant const& value) -> void;

    // static ui lookups
    Q_INVOKABLE auto get_value(QString const& path) const -> QVariant;
    Q_INVOKABLE auto set_value(QString const& path, QVariant const& value, bool mergeable) -> void;

    /// called by command's notify lambda when value actually changes
    auto update_node_value(int_t row, QVariant const& new_value) -> void;

    /// rebuilds UI list from reflected configuration
    ///
    /// \pre config must outlive property model
    template <typename config_t, typename predicate_t>
    auto load_config(config_t& config, predicate_t path_predicate) -> void
    {
        beginResetModel();
        nodes_.clear();

        auto inspector = hierarchical_inspector_factory_(
            [&](std::string_view path, auto& modified_param) {
                using param_t = std::remove_cvref_t<decltype(modified_param)>;
                using value_t = param_t::value_t;

                auto const row = static_cast<int_t>(size(nodes_));

                auto min = QVariant{};
                if constexpr (requires { modified_param.constraint().min; })
                {
                    min = QVariant::fromValue(modified_param.constraint().min);
                }

                auto max = QVariant{};
                if constexpr (requires { modified_param.constraint().max; })
                {
                    max = QVariant::fromValue(modified_param.constraint().max);
                }

                auto push_command = [&, row](QVariant const& src, bool mergeable) {
                    auto const& next = src.template value<value_t>();

                    // guard against infinite loops triggered by UI bindings re-evaluating
                    if (next == modified_param.value()) return;

                    undo_stack_->emplace_now<command::mutate_param_t<param_t>>(
                        mergeable, modified_param, next, [=, this](param_t& command_param, value_t const& cur) {
                            // guard against infinite loops triggered by UI bindings re-evaluating
                            if (cur == command_param.value()) return;

                            update_node_value(row, to_variant(command_param.value()));
                        });
                };

                std::optional<property_type_id_t> property_type_id;
                static_assert(std::floating_point<value_t> || std::signed_integral<value_t>
                    || std::same_as<value_t, bool> || is_enum<value_t> || std::same_as<value_t, std::string>);
                if constexpr (std::floating_point<value_t>) property_type_id = property_type_id_t::float_t;
                else if constexpr (std::signed_integral<value_t>) property_type_id = property_type_id_t::int_t;
                else if constexpr (std::same_as<value_t, bool>) property_type_id = property_type_id_t::bool_t;

                if constexpr (!std::same_as<value_t, std::string> && !is_enum<value_t>)
                {
                    if (property_type_id.has_value())
                    {
                        auto ui_node = ui_node_t{
                            .path = QString::fromLocal8Bit(std::data(path), std::size(path)),
                            .type = *property_type_id,
                            .value = to_variant(modified_param.value()),
                            .min = min,
                            .max = max,
                            .push_command = std::move(push_command),
                        };
                        nodes_.emplace_back(std::move(ui_node));
                    }
                }
            },
            std::move(path_predicate));
        config.reflect(inspector);

        endResetModel();
    }

    template <typename config_t> auto load_config(config_t& config) -> void
    {
        return load_config(config, [](auto const&) static noexcept { return true; });
    }

signals:
    // emitted when value changes via undo stack so static ui can update
    void valueChanged(QString path, QVariant value);

private:
    static constexpr auto to_variant(auto const& val) -> QVariant
    {
        using raw_t = std::remove_cvref_t<decltype(val)>;
        if constexpr (std::same_as<raw_t, bool>) return QVariant::fromValue(val);
        else if constexpr (std::signed_integral<raw_t>) return QVariant::fromValue(static_cast<qint64>(val));
        else return QVariant::fromValue(val);
    }

    undo_stack_t* undo_stack_;
    hierarchical_inspector_factory_t hierarchical_inspector_factory_;
    std::vector<ui_node_t> nodes_;
};

} // namespace crv
