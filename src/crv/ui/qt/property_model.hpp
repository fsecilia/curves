// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/float_limits.hpp>
#include <crv/math/limits.hpp>
#include <crv/ui/command/command.hpp>
#include <crv/ui/command/mutate_param.hpp>
#include <crv/ui/hierarchical_inspector.hpp>
#include <QAbstractListModel>
#include <QString>
#include <QUndoStack>
#include <QVariant>
#include <functional>
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
    QVariant min_val;
    QVariant max_val;

    // executes the command logic, passing the new value from QML
    std::function<auto(QVariant const&)->void> push_command;
};

class curve_property_model_t : public QAbstractListModel
{
    Q_OBJECT

public:
    enum roles
    {
        PathRole = Qt::UserRole + 1,
        TypeRole,
        ValueRole,
        MinRole,
        MaxRole
    };

    explicit curve_property_model_t(QUndoStack& undo_stack,
        hierarchical_inspector_factory_t hierarchical_inspector_factory, command::factory_t command_factory,
        QObject* parent = nullptr);

    auto rowCount(QModelIndex const& parent = QModelIndex()) const -> int override;
    auto data(QModelIndex const& index, int role = Qt::DisplayRole) const -> QVariant override;
    auto setData(QModelIndex const& index, QVariant const& value, int role = Qt::EditRole) -> bool override;
    auto roleNames() const -> QHash<int, QByteArray> override;

    /// called by the command's notify lambda when a value successfully changes
    auto update_node_value(int_t row, QVariant const& new_value) -> void;

    /// rebuilds UI list from a reflected configuration
    ///
    /// \pre config must outlive property model
    template <typename config_t> auto load_config(config_t& config) -> void
    {
        beginResetModel();
        nodes_.clear();

        auto const inspector = hierarchical_inspector_factory_([&](std::string_view path, auto& param) {
            using param_t = std::remove_cv_t<decltype(param)>;
            using value_t = param_t::value_t;

            auto const row = static_cast<int_t>(size(nodes_));

            auto min = QVariant{};
            if constexpr (requires { param.constraint().min; }) min = QVariant::fromValue(param.constraint().min);

            auto max = QVariant{};
            if constexpr (requires { param.constraint().max; }) max = QVariant::fromValue(param.constraint().max);

            auto push_command = [&](QVariant const& src) {
                auto const& next = src.template value<value_t>();
                if (next == param.value()) return;

                undo_stack_->push(command_factory_.template create<command::mutate_param_t<param_t>>(
                    param, next, [&](param_t& param, value_t const& cur) {
                        if (cur == param.value()) return;
                        update_node_value(row, src);
                    }));
            };

            property_type_id_t property_type_id;
            if constexpr (std::floating_point<value_t>) property_type_id = property_type_id_t::float_t;
            else if constexpr (std::signed_integral<value_t>) property_type_id = property_type_id_t::int_t;
            else if constexpr (std::same_as<value_t, bool>) property_type_id = property_type_id_t::bool_t;
            else static_assert(false);

            nodes_.emplace_back(QString::fromLocal8Bit(std::data(path), std::size(path)), property_type_id,
                param.value(), min, max, std::move(push_command));
        });
        inspector.inspect(config);

        endResetModel();
    }

private:
    QUndoStack* undo_stack_;
    hierarchical_inspector_factory_t hierarchical_inspector_factory_;
    command::factory_t command_factory_;
    std::vector<ui_node_t> nodes_;
};

} // namespace crv
