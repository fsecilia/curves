// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "property_model.hpp"
#include <crv/math/integer.hpp>

namespace crv {

curve_property_model_t::curve_property_model_t(
    undo_stack_t& undo_stack, hierarchical_inspector_factory_t hierarchical_inspector_factory, QObject* parent)
    : QAbstractListModel{parent}, undo_stack_{&undo_stack},
      hierarchical_inspector_factory_{std::move(hierarchical_inspector_factory)}
{}

auto curve_property_model_t::rowCount(QModelIndex const& parent) const -> int
{
    if (parent.isValid()) return 0;
    return static_cast<int>(nodes_.size());
}

auto curve_property_model_t::data(QModelIndex const& index, int role) const -> QVariant
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
    }

    assert(false && "unexpected role");
    return {};
}

auto curve_property_model_t::setData(QModelIndex const& index, QVariant const& value, int role) -> bool
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
    node.push_command(value);

    return true;
}

auto curve_property_model_t::roleNames() const -> QHash<int, QByteArray>
{
    auto roles = QHash<int, QByteArray>{};
    roles[static_cast<int>(roles_t::path)] = "path";
    roles[static_cast<int>(roles_t::type)] = "typeId";
    roles[static_cast<int>(roles_t::value)] = "value";
    roles[static_cast<int>(roles_t::min)] = "minVal";
    roles[static_cast<int>(roles_t::max)] = "maxVal";
    return roles;
}

auto curve_property_model_t::update_node_value(int_t row, QVariant const& new_value) -> void
{
    if (row < 0 || static_cast<int_t>(nodes_.size()) <= row) return;

    nodes_[row].value = new_value;

    // notify QML that this specific row's value has changed
    auto const model_index = index(int_cast<int>(row), 0);
    emit dataChanged(model_index, model_index, {static_cast<int>(roles_t::value)});
}

} // namespace crv
