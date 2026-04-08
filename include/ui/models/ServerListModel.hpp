// SPDX-License-Identifier: GPL-2.0-or-later
// ServerListModel — QAbstractListModel for proxy server list.
// Provides data for QListView/QTableView with 10 000+ profiles without lag.
// Supports drag-and-drop reordering, filtering, role-based access.

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QMimeData>
#include <memory>

#include "include/dataStore/ProxyEntity.hpp"

class ServerListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum ServerRoles {
        NameRole = Qt::UserRole + 1,
        AddressRole,
        PortRole,
        TypeRole,
        LatencyRole,
        TrafficUpRole,
        TrafficDownRole,
        GroupIdRole,
        ProfileIdRole,
        IsActiveRole,
    };
    Q_ENUM(ServerRoles)

    explicit ServerListModel(QObject *parent = nullptr);

    // --- QAbstractListModel interface ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // --- Drag & Drop ---
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    // --- Data management ---
    void setProfiles(const QList<std::shared_ptr<Configs::ProxyEntity>> &profiles);
    void appendProfile(const std::shared_ptr<Configs::ProxyEntity> &profile);
    void removeProfile(int profileId);
    void updateProfile(int profileId, const std::shared_ptr<Configs::ProxyEntity> &profile);

    // Mark a profile as the currently active connection.
    void setActiveProfileId(int id);
    int activeProfileId() const { return m_activeId; }

    // Update latency result for a profile.
    void setLatency(int profileId, int latencyMs);

    // Update traffic counters.
    void setTraffic(int profileId, qint64 up, qint64 down);

    // Access underlying data (for persistence).
    QList<int> profileOrder() const;

signals:
    void orderChanged();

private:
    int rowForProfileId(int id) const;

    struct ServerEntry {
        std::shared_ptr<Configs::ProxyEntity> entity;
        int latencyMs = -1;
        qint64 trafficUp = 0;
        qint64 trafficDown = 0;
    };

    QList<ServerEntry> m_entries;
    int m_activeId = -1;
};
