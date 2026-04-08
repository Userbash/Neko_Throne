// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/ui/models/RoutingRulesModel.hpp"

#include <QDataStream>
#include <QIODevice>
#include <algorithm>

static constexpr char MIME_TYPE[] = "application/x-throne-rule-ids";

RoutingRulesModel::RoutingRulesModel(QObject *parent)
    : QAbstractListModel(parent) {}

int RoutingRulesModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_rules.size();
}

QVariant RoutingRulesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rules.size())
        return {};

    const auto &r = m_rules[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:      return r.name;
    case RuleIdRole:    return r.id;
    case TypeRole:      return r.type;
    case PatternRole:   return r.pattern;
    case OutboundRole:  return r.outbound;
    case EnabledRole:   return r.enabled;
    }
    return {};
}

bool RoutingRulesModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.row() >= m_rules.size())
        return false;

    auto &r = m_rules[index.row()];
    switch (role) {
    case EnabledRole:
        r.enabled = value.toBool();
        emit dataChanged(index, index, {EnabledRole});
        emit rulesChanged();
        return true;
    case NameRole:
        r.name = value.toString();
        emit dataChanged(index, index, {NameRole});
        emit rulesChanged();
        return true;
    }
    return false;
}

QHash<int, QByteArray> RoutingRulesModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[RuleIdRole]   = "ruleId";
    roles[NameRole]     = "name";
    roles[TypeRole]     = "type";
    roles[PatternRole]  = "pattern";
    roles[OutboundRole] = "outbound";
    roles[EnabledRole]  = "enabled";
    return roles;
}

Qt::ItemFlags RoutingRulesModel::flags(const QModelIndex &index) const {
    auto f = QAbstractListModel::flags(index);
    if (index.isValid())
        return f | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
    return f | Qt::ItemIsDropEnabled;
}

Qt::DropActions RoutingRulesModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QStringList RoutingRulesModel::mimeTypes() const {
    return {QString::fromLatin1(MIME_TYPE)};
}

QMimeData *RoutingRulesModel::mimeData(const QModelIndexList &indexes) const {
    auto *mimeData = new QMimeData;
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    for (const auto &idx : indexes) {
        if (idx.isValid())
            stream << idx.row();
    }
    mimeData->setData(QString::fromLatin1(MIME_TYPE), encoded);
    return mimeData;
}

bool RoutingRulesModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                     int row, int /*column*/, const QModelIndex &parent) {
    if (action == Qt::IgnoreAction)
        return true;
    if (!data->hasFormat(QString::fromLatin1(MIME_TYPE)))
        return false;

    int destRow = row < 0 ? (parent.isValid() ? parent.row() : rowCount()) : row;

    QByteArray encoded = data->data(QString::fromLatin1(MIME_TYPE));
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    QList<int> sourceRows;
    while (!stream.atEnd()) {
        int r;
        stream >> r;
        sourceRows.append(r);
    }

    std::sort(sourceRows.begin(), sourceRows.end(), std::greater<>());

    QList<RoutingRule> moved;
    for (int r : sourceRows)
        moved.prepend(m_rules[r]);

    for (int r : sourceRows) {
        beginRemoveRows({}, r, r);
        m_rules.removeAt(r);
        endRemoveRows();
        if (r < destRow)
            --destRow;
    }

    beginInsertRows({}, destRow, destRow + moved.size() - 1);
    for (int i = 0; i < moved.size(); ++i)
        m_rules.insert(destRow + i, moved[i]);
    endInsertRows();

    emit rulesChanged();
    return true;
}

void RoutingRulesModel::setRules(const QList<RoutingRule> &rules) {
    beginResetModel();
    m_rules = rules;
    endResetModel();
}

void RoutingRulesModel::appendRule(const RoutingRule &rule) {
    int row = m_rules.size();
    beginInsertRows({}, row, row);
    m_rules.append(rule);
    endInsertRows();
    emit rulesChanged();
}

void RoutingRulesModel::removeRule(int ruleId) {
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == ruleId) {
            beginRemoveRows({}, i, i);
            m_rules.removeAt(i);
            endRemoveRows();
            emit rulesChanged();
            return;
        }
    }
}
