// SPDX-License-Identifier: GPL-2.0-or-later
// RoutingRulesModel — QAbstractListModel for routing rules.
// Supports drag-and-drop reordering and role-based data access.

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QMimeData>
#include <QString>

struct RoutingRule {
    int id = 0;
    QString name;
    QString type;       // "domain", "ip", "port", "process", etc.
    QString pattern;    // the actual rule pattern
    QString outbound;   // "proxy", "direct", "block"
    bool enabled = true;
};

class RoutingRulesModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum RuleRoles {
        RuleIdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        PatternRole,
        OutboundRole,
        EnabledRole,
    };
    Q_ENUM(RuleRoles)

    explicit RoutingRulesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Drag & Drop
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    void setRules(const QList<RoutingRule> &rules);
    void appendRule(const RoutingRule &rule);
    void removeRule(int ruleId);
    QList<RoutingRule> rules() const { return m_rules; }

signals:
    void rulesChanged();

private:
    QList<RoutingRule> m_rules;
};
