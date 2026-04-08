// SPDX-License-Identifier: GPL-2.0-or-later
// LogModel — Ring-buffer backed QAbstractListModel for live log display.
// Caps at max_lines to prevent unbounded memory growth.
// Thread-safe: appendLog() can be called from any thread via queued connection.

#pragma once

#include <QAbstractListModel>
#include <QMutex>
#include <QColor>
#include <vector>

class LogModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum LogRoles {
        TimestampRole = Qt::UserRole + 1,
        LevelRole,
        MessageRole,
        ColorRole,
    };
    Q_ENUM(LogRoles)

    explicit LogModel(QObject *parent = nullptr, int maxLines = 5000);

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Thread-safe append.
    Q_INVOKABLE void appendLog(const QString &timestamp, const QString &level, const QString &message);

    // Clear all entries.
    Q_INVOKABLE void clear();

    int maxLines() const { return static_cast<int>(m_maxLines); }
    void setMaxLines(int n);

signals:
    void logAppended();

private:
    struct LogEntry {
        QString timestamp;
        QString level;
        QString message;
    };

    static QColor colorForLevel(const QString &level);

    std::vector<LogEntry> m_buffer;
    std::size_t m_head = 0;
    std::size_t m_count = 0;
    std::size_t m_maxLines;
    mutable QMutex m_mutex;

    const LogEntry &entryAt(std::size_t logicalIndex) const;
};
