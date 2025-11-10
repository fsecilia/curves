#include "CurveParameterModel.hpp"

CurveParameterModel::CurveParameterModel(QObject* parent)
    : QAbstractListModel(parent) {
  // Constructor
}

// Returns the number of items in the list.
auto CurveParameterModel::rowCount(const QModelIndex& parent) const -> int {
  // This model is a flat list, so we only return items
  // if the 'parent' index is invalid.
  if (parent.isValid()) {
    return 0;
  }
  return m_parameters.count();
}

// Returns the data for a specific item (index) and role (column).
auto CurveParameterModel::data(const QModelIndex& index, int role) const
    -> QVariant {
  if (!index.isValid() || index.row() >= m_parameters.count()) {
    return QVariant();
  }

  // Get the parameter for the requested row
  const auto& param = m_parameters.at(index.row());

  // Switch on the requested role
  switch (role) {
    case LabelRole:
      return param.label;
    case ValueRole:
      return param.value;
    case TypeRole:
      return param.type;
  }

  // Return an invalid QVariant if the role isn't recognized
  return QVariant();
}

/**
 * @brief This function maps the C++ enum roles to byte-array (string) names
 * that QML can use in its bindings.
 */
auto CurveParameterModel::roleNames() const -> QHash<int, QByteArray> {
  QHash<int, QByteArray> roles;

  // QML's 'model.label' will be mapped to LabelRole
  roles[LabelRole] = "label";

  // QML's 'model.value' will be mapped to ValueRole
  roles[ValueRole] = "value";

  // QML's 'model.type' will be mapped to TypeRole
  roles[TypeRole] = "type";

  return roles;
}

/**
 * @brief Public function for the EditorPresenter to update the list.
 */
auto CurveParameterModel::setParameters(const QList<CurveParameter>& params)
    -> void {
  // This tells the connected ListView that the model is
  // about to be completely reset (e.g., cleared and repopulated).
  beginResetModel();

  m_parameters = params;

  // This tells the ListView that the reset is complete and
  // it should request the new data and redraw.
  endResetModel();
}
