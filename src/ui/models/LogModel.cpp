// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/ui/models/LogModel.hpp"

#include <QMutexLocker>

LogModel::LogModel(QObject *parent, int maxLines)
    : QAbstractListModel(parent)
    , m_maxLines(static_cast<std::size_t>(maxLines))
{
    m_buffer.resize(m_maxLines);
}

int LogModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_count);
}

QVariant LogModel::data(const QModelIndex &index, int role) const {
    QMutexLocker lock(&m_mutex);
    if (!index.isValid() || static_cast<std::size_t>(index.row()) >= m_count)
        return {};

    const auto &entry = entryAt(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case MessageRole:     return entry.message;
    case TimestampRole:   return entry.timestamp;
    case LevelRole:       return entry.level;
    case ColorRole:       return colorForLevel(entry.level);
    }
    return {};
}

QHash<int, QByteArray> LogModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[TimestampRole] = "timestamp";
    roles[LevelRole]     = "level";
    roles[MessageRole]   = "message";
    roles[ColorRole]     = "color";
    return roles;
}

void LogModel::appendLog(const QString &timestamp, const QString &level, const QString &message) {
    {
        QMutexLocker lock(&m_mutex);

        m_buffer[m_head] = {timestamp, level, message};
        m_head = (m_head + 1) % m_maxLines;

        if (m_count < m_maxLines) {
            // Growing phase: we can signal insertions.
            lock.unlock();
            int row = static_cast<int>(m_count);
            beginInsertRows({}, row, row);
            {
                QMutexLocker lock2(&m_mutex);
                ++m_count;
            }
            endInsertRows();
        } else {
            // Buffer is full: the oldest entry was overwritten.
            // Reset model because logical row 0 changed.
            lock.unlock();
            beginResetModel();
            endResetModel();
        }
    }
    emit logAppended();
}

void LogModel::clear() {
    QMutexLocker lock(&m_mutex);
    beginResetModel();
    m_head = 0;
    m_count = 0;
    endResetModel();
}

void LogModel::setMaxLines(int n) {
    QMutexLocker lock(&m_mutex);
    beginResetModel();
    m_maxLines = static_cast<std::size_t>(n);
    m_buffer.resize(m_maxLines);
    m_head = 0;
    m_count = 0;
    endResetModel();
}

const LogModel::LogEntry &LogModel::entryAt(std::size_t logicalIndex) const {
    std::size_t realIndex;
    if (m_count < m_maxLines)
        realIndex = logicalIndex;
    else
        realIndex = (m_head + logicalIndex) % m_maxLines;
    return m_buffer[realIndex];
}

QColor LogModel::colorForLevel(const QString &level) {
    if (level == QStringLiteral("error") || level == QStringLiteral("fatal"))
        return {0xFF, 0x5C, 0x5C};  // red
    if (level == QStringLiteral("warn") || level == QStringLiteral("warning"))
        return {0xFF, 0xC1, 0x07};  // amber
    if (level == QStringLiteral("debug") || level == QStringLiteral("trace"))
        return {0x78, 0x78, 0x78};  // gray
    return {0xDD, 0xDD, 0xDD};      // white-ish for info
}
