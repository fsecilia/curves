#include "CurveTypeModel.hpp"

CurveTypeModel::CurveTypeModel(QObject* parent) : QAbstractListModel(parent) {
  // Constructor
}

// Returns the number of items in the list.
auto CurveTypeModel::rowCount(const QModelIndex& parent) const -> int {
  if (parent.isValid()) {
    return 0;  // This is a flat list
  }
  return m_curveNames.count();
}

// Returns the data for a specific item (index) and role (column).
auto CurveTypeModel::data(const QModelIndex& index, int role) const
    -> QVariant {
  if (!index.isValid() || index.row() >= m_curveNames.count()) {
    return QVariant();
  }

  const auto& name = m_curveNames.at(index.row());

  // Switch on the requested role
  switch (role) {
    case DisplayNameRole:
      return name;  // Return the string
    case CurveIdRole:
      // Return a simple ID (e.g., lowercase)
      return name.toLower();
  }

  return QVariant();  // Invalid role
}

/**
 * @brief Maps the C++ enum roles to string names that QML can use.
 */
auto CurveTypeModel::roleNames() const -> QHash<int, QByteArray> {
  QHash<int, QByteArray> roles;
  roles[DisplayNameRole] = "displayName";
  roles[CurveIdRole] = "curveId";
  return roles;
}

/**
 * @brief Public function to populate the model from the presenter.
 */
auto CurveTypeModel::populate(const QList<QString>& curveNames) -> void {
  // Tell the view that the model is about to be completely replaced
  beginResetModel();

  m_curveNames = curveNames;

  // Tell the view that the model has been replaced
  endResetModel();
}
