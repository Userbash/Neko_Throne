// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/ui/models/ServerListModel.hpp"

#include <QDataStream>
#include <QIODevice>

static constexpr char MIME_TYPE[] = "application/x-throne-server-ids";

ServerListModel::ServerListModel(QObject *parent)
    : QAbstractListModel(parent) {}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int ServerListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ServerListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const auto &entry = m_entries[index.row()];
    const auto &e = entry.entity;

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:       return e->_bean->DisplayName();
    case AddressRole:    return e->_bean->DisplayAddress();
    case PortRole:       return e->_bean->DisplayPort();
    case TypeRole:       return e->_bean->DisplayType();
    case LatencyRole:    return entry.latencyMs;
    case TrafficUpRole:  return entry.trafficUp;
    case TrafficDownRole:return entry.trafficDown;
    case GroupIdRole:    return e->gid;
    case ProfileIdRole:  return e->id;
    case IsActiveRole:   return e->id == m_activeId;
    }
    return {};
}

QHash<int, QByteArray> ServerListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[NameRole]        = "name";
    roles[AddressRole]     = "address";
    roles[PortRole]        = "port";
    roles[TypeRole]        = "type";
    roles[LatencyRole]     = "latency";
    roles[TrafficUpRole]   = "trafficUp";
    roles[TrafficDownRole] = "trafficDown";
    roles[GroupIdRole]     = "groupId";
    roles[ProfileIdRole]   = "profileId";
    roles[IsActiveRole]    = "isActive";
    return roles;
}

Qt::ItemFlags ServerListModel::flags(const QModelIndex &index) const {
    auto defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid())
        return defaultFlags | Qt::ItemIsDragEnabled;
    return defaultFlags | Qt::ItemIsDropEnabled;
}

// ---------------------------------------------------------------------------
// Drag & Drop
// ---------------------------------------------------------------------------

Qt::DropActions ServerListModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QStringList ServerListModel::mimeTypes() const {
    return {QString::fromLatin1(MIME_TYPE)};
}

QMimeData *ServerListModel::mimeData(const QModelIndexList &indexes) const {
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

bool ServerListModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                   int row, int /*column*/, const QModelIndex &parent) {
    if (action == Qt::IgnoreAction)
        return true;
    if (!data->hasFormat(QString::fromLatin1(MIME_TYPE)))
        return false;

    int destRow = row;
    if (destRow < 0) {
        destRow = parent.isValid() ? parent.row() : rowCount();
    }

    QByteArray encoded = data->data(QString::fromLatin1(MIME_TYPE));
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    QList<int> sourceRows;
    while (!stream.atEnd()) {
        int r;
        stream >> r;
        sourceRows.append(r);
    }

    // Sort descending so we can remove without invalidating indices.
    std::sort(sourceRows.begin(), sourceRows.end(), std::greater<>());

    QList<ServerEntry> movedEntries;
    for (int r : sourceRows) {
        movedEntries.prepend(m_entries[r]);
    }

    // Remove from model
    for (int r : sourceRows) {
        beginRemoveRows({}, r, r);
        m_entries.removeAt(r);
        endRemoveRows();
        if (r < destRow)
            --destRow;
    }

    // Insert at destination
    beginInsertRows({}, destRow, destRow + movedEntries.size() - 1);
    for (int i = 0; i < movedEntries.size(); ++i) {
        m_entries.insert(destRow + i, movedEntries[i]);
    }
    endInsertRows();

    emit orderChanged();
    return true;
}

// ---------------------------------------------------------------------------
// Data management
// ---------------------------------------------------------------------------

void ServerListModel::setProfiles(const QList<std::shared_ptr<Configs::ProxyEntity>> &profiles) {
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(profiles.size());
    for (const auto &p : profiles) {
        m_entries.append({p, -1, 0, 0});
    }
    endResetModel();
}

void ServerListModel::appendProfile(const std::shared_ptr<Configs::ProxyEntity> &profile) {
    int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append({profile, -1, 0, 0});
    endInsertRows();
}

void ServerListModel::removeProfile(int profileId) {
    int row = rowForProfileId(profileId);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_entries.removeAt(row);
    endRemoveRows();
}

void ServerListModel::updateProfile(int profileId, const std::shared_ptr<Configs::ProxyEntity> &profile) {
    int row = rowForProfileId(profileId);
    if (row < 0)
        return;
    m_entries[row].entity = profile;
    emit dataChanged(index(row), index(row));
}

void ServerListModel::setActiveProfileId(int id) {
    int oldRow = rowForProfileId(m_activeId);
    int newRow = rowForProfileId(id);
    m_activeId = id;
    if (oldRow >= 0)
        emit dataChanged(index(oldRow), index(oldRow), {IsActiveRole});
    if (newRow >= 0)
        emit dataChanged(index(newRow), index(newRow), {IsActiveRole});
}

void ServerListModel::setLatency(int profileId, int latencyMs) {
    int row = rowForProfileId(profileId);
    if (row < 0)
        return;
    m_entries[row].latencyMs = latencyMs;
    emit dataChanged(index(row), index(row), {LatencyRole});
}

void ServerListModel::setTraffic(int profileId, qint64 up, qint64 down) {
    int row = rowForProfileId(profileId);
    if (row < 0)
        return;
    m_entries[row].trafficUp = up;
    m_entries[row].trafficDown = down;
    emit dataChanged(index(row), index(row), {TrafficUpRole, TrafficDownRole});
}

QList<int> ServerListModel::profileOrder() const {
    QList<int> order;
    order.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        order.append(entry.entity->id);
    }
    return order;
}

int ServerListModel::rowForProfileId(int id) const {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].entity->id == id)
            return i;
    }
    return -1;
}
