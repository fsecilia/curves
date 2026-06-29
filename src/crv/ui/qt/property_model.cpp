// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "property_model.hpp"
#include <crv/math/integer.hpp>
#include <algorithm>

namespace crv {

property_model_t::property_model_t(
    undo_stack_t& undo_stack, hierarchical_inspector_factory_t hierarchical_inspector_factory, QObject* parent)
    : QAbstractListModel{parent}, undo_stack_{&undo_stack},
      hierarchical_inspector_factory_{std::move(hierarchical_inspector_factory)}
{}

auto property_model_t::rowCount(QModelIndex const& parent) const -> int
{
    if (parent.isValid()) return 0;
    return static_cast<int>(nodes_.size());
}

auto property_model_t::data(QModelIndex const& index, int role) const -> QVariant
{
    if (!index.isValid() || index.row() >= static_cast<int>(nodes_.size())) { return {}; }

    auto const& node = nodes_[index.row()];

    switch (static_cast<roles_t>(role))
    {
        case roles_t::path: return node.path;
        case roles_t::type: return static_cast<int>(node.type);
        case roles_t::value: return node.value;
        case roles_t::min: return node.min;
        case roles_t::max: return node.max;
        case roles_t::error_message: return node.error_message;
    }

    assert(false && "unexpected role");
    return {};
}

auto property_model_t::setData(QModelIndex const& index, QVariant const& value, int role) -> bool
{
    if (!index.isValid() || static_cast<int>(nodes_.size()) <= index.row()
        || role != static_cast<int_t>(roles_t::value))
    {
        return false;
    }

    auto const& node = nodes_[index.row()];

    // guard against infinite loops triggered by UI bindings re-evaluating
    if (node.value == value) return false;

    // fire the type-erased command
    node.push_command(value, false);

    return true;
}

auto property_model_t::roleNames() const -> QHash<int, QByteArray>
{
    auto roles = QHash<int, QByteArray>{};
    roles[static_cast<int>(roles_t::path)] = "path";
    roles[static_cast<int>(roles_t::type)] = "typeId";
    roles[static_cast<int>(roles_t::value)] = "value";
    roles[static_cast<int>(roles_t::min)] = "min";
    roles[static_cast<int>(roles_t::max)] = "max";
    roles[static_cast<int>(roles_t::error_message)] = "errorMessage";
    return roles;
}

auto property_model_t::on_wheel(int row, QVariant const& value) -> void
{
    if (row < 0 || static_cast<int>(nodes_.size()) <= row) return;
    nodes_[row].push_command(value, true);
}

auto property_model_t::get_value(QString const& path) const -> QVariant
{
    auto const it = std::ranges::find_if(nodes_, [&](auto const& node) { return node.path == path; });
    assert(it != nodes_.end());
    if (it != nodes_.end()) return it->value;
    return {};
}

auto property_model_t::set_value(QString const& path, QVariant const& value, bool mergeable) -> void
{
    auto const it = std::ranges::find_if(nodes_, [&](auto const& node) { return node.path == path; });
    assert(it != nodes_.end());
    if (it == nodes_.end()) return;

    // guard against infinite binding loops
    if (it->value == value) return;
    it->push_command(value, mergeable);
}

auto property_model_t::update_node_value(int_t row, QVariant const& new_value) -> void
{
    if (row < 0 || static_cast<int_t>(nodes_.size()) <= row) return;

    nodes_[row].value = new_value;

    // notify QML that this specific row's data has changed
    auto const model_index = index(int_cast<int>(row), 0);
    emit dataChanged(model_index, model_index, {static_cast<int>(roles_t::value)});

    // notify static ui layouts
    emit valueChanged(nodes_[row].path, new_value);
}

auto property_model_t::error_message(QString const& path, QString const& message) -> void
{
    auto const it = std::ranges::find_if(nodes_, [&](auto const& node) { return node.path == path; });
    assert(it != nodes_.end());
    if (it == nodes_.end()) return;

    it->error_message = message;

    auto const model_index = index(int_cast<int>(std::ranges::distance(nodes_.begin(), it)), 0);
    emit dataChanged(model_index, model_index, {static_cast<int>(roles_t::error_message)});
}

} // namespace crv
