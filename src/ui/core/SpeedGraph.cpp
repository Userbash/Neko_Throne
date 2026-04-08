// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/ui/core/SpeedGraph.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QtMath>
#include <algorithm>

SpeedGraph::SpeedGraph(QWidget *parent, int bufferSeconds)
    : QWidget(parent)
{
    Q_UNUSED(bufferSeconds);
    setMinimumSize(200, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_repaintTimer.setTimerType(Qt::PreciseTimer);
    m_repaintTimer.setInterval(m_refreshMs);
    connect(&m_repaintTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_repaintTimer.start();
}

// ---------------------------------------------------------------------------
// Ring buffer operations
// ---------------------------------------------------------------------------

void SpeedGraph::push(const DataPoint &pt) {
    m_buffer[m_head] = pt;
    m_head = (m_head + 1) % MAX_POINTS;
    if (m_count < MAX_POINTS)
        ++m_count;
}

SpeedGraph::DataPoint SpeedGraph::at(std::size_t logicalIndex) const {
    // logicalIndex 0 = oldest, logicalIndex count-1 = newest
    std::size_t realIndex;
    if (m_count < MAX_POINTS)
        realIndex = logicalIndex;
    else
        realIndex = (m_head + logicalIndex) % MAX_POINTS;
    return m_buffer[realIndex];
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SpeedGraph::addDataPoint(qint64 downloadSpeed, qint64 uploadSpeed) {
    DataPoint pt{downloadSpeed, uploadSpeed};
    push(pt);

    if (downloadSpeed > m_peakDownload || uploadSpeed > m_peakUpload) {
        m_peakDownload = std::max(m_peakDownload, downloadSpeed);
        m_peakUpload = std::max(m_peakUpload, uploadSpeed);
        emit peakSpeedChanged(m_peakDownload, m_peakUpload);
    }
}

void SpeedGraph::clear() {
    m_head = 0;
    m_count = 0;
    m_peakDownload = 0;
    m_peakUpload = 0;
    m_buffer.fill({});
    update();
}

void SpeedGraph::setRefreshRate(int ms) {
    m_refreshMs = std::max(8, ms); // cap at 125 FPS
    m_repaintTimer.setInterval(m_refreshMs);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void SpeedGraph::paintEvent(QPaintEvent * /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = rect().adjusted(0, 0, 0, 0);

    // Background
    p.fillRect(area, QColor(20, 20, 28, 200));

    if (m_count < 2) {
        drawLegend(p, area);
        return;
    }

    const QRectF graphArea = area.adjusted(50, 16, -16, -28);

    drawGrid(p, graphArea);
    drawGraph(p, graphArea, false); // download
    drawGraph(p, graphArea, true);  // upload
    drawLegend(p, area);
}

void SpeedGraph::drawGrid(QPainter &p, const QRectF &area) {
    p.setPen(QPen(m_gridColor, 1.0, Qt::DotLine));

    // Horizontal grid lines (4 divisions)
    for (int i = 1; i < 4; ++i) {
        qreal y = area.top() + area.height() * i / 4.0;
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    // Y-axis labels
    qint64 maxVal = std::max(m_peakDownload, m_peakUpload);
    if (maxVal <= 0) maxVal = 1024; // 1 KB/s minimum scale

    p.setPen(QColor(180, 180, 180));
    QFont smallFont = font();
    smallFont.setPixelSize(10);
    p.setFont(smallFont);

    for (int i = 0; i <= 4; ++i) {
        qint64 val = maxVal * (4 - i) / 4;
        qreal y = area.top() + area.height() * i / 4.0;
        p.drawText(QRectF(0, y - 8, area.left() - 4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, formatSpeed(val));
    }
}

void SpeedGraph::drawGraph(QPainter &p, const QRectF &area, bool isUpload) {
    if (m_count < 2)
        return;

    qint64 maxVal = std::max(m_peakDownload, m_peakUpload);
    if (maxVal <= 0) maxVal = 1024;

    // How many points to draw? Use visible width, at most m_count.
    const int visiblePoints = std::min(static_cast<std::size_t>(area.width()), m_count);
    const std::size_t startIdx = m_count - visiblePoints;
    const qreal xStep = area.width() / static_cast<qreal>(visiblePoints - 1);

    QPainterPath path;
    QPainterPath fillPath;

    for (int i = 0; i < visiblePoints; ++i) {
        DataPoint dp = at(startIdx + i);
        qint64 val = isUpload ? dp.upload : dp.download;
        qreal x = area.left() + i * xStep;
        qreal y = area.bottom() - (static_cast<qreal>(val) / maxVal) * area.height();
        y = std::clamp(y, area.top(), area.bottom());

        if (i == 0) {
            path.moveTo(x, y);
            fillPath.moveTo(x, area.bottom());
            fillPath.lineTo(x, y);
        } else {
            path.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }

    // Close fill path
    fillPath.lineTo(area.right(), area.bottom());
    fillPath.closeSubpath();

    // Fill gradient
    QColor fillColor = isUpload ? m_uploadColor : m_downloadColor;
    fillColor.setAlpha(40);
    p.fillPath(fillPath, fillColor);

    // Line
    QColor lineColor = isUpload ? m_uploadColor : m_downloadColor;
    p.setPen(QPen(lineColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
}

void SpeedGraph::drawLegend(QPainter &p, const QRectF &area) {
    QFont legendFont = font();
    legendFont.setPixelSize(11);
    p.setFont(legendFont);

    // Current values (last data point)
    qint64 curDown = 0, curUp = 0;
    if (m_count > 0) {
        DataPoint last = at(m_count - 1);
        curDown = last.download;
        curUp = last.upload;
    }

    const qreal y = area.bottom() - 14;
    const qreal leftX = area.left() + 54;

    // Download indicator
    p.setPen(m_downloadColor);
    p.fillRect(QRectF(leftX, y + 2, 10, 10), m_downloadColor);
    p.drawText(QPointF(leftX + 14, y + 12),
               QStringLiteral("↓ ") + formatSpeed(curDown));

    // Upload indicator
    p.setPen(m_uploadColor);
    p.fillRect(QRectF(leftX + 140, y + 2, 10, 10), m_uploadColor);
    p.drawText(QPointF(leftX + 154, y + 12),
               QStringLiteral("↑ ") + formatSpeed(curUp));
}

void SpeedGraph::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

QString SpeedGraph::formatSpeed(qint64 bytesPerSec) {
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = 1024 * KB;
    constexpr qint64 GB = 1024 * MB;

    if (bytesPerSec >= GB)
        return QString::number(static_cast<double>(bytesPerSec) / GB, 'f', 2) + QStringLiteral(" GB/s");
    if (bytesPerSec >= MB)
        return QString::number(static_cast<double>(bytesPerSec) / MB, 'f', 1) + QStringLiteral(" MB/s");
    if (bytesPerSec >= KB)
        return QString::number(static_cast<double>(bytesPerSec) / KB, 'f', 1) + QStringLiteral(" KB/s");
    return QString::number(bytesPerSec) + QStringLiteral(" B/s");
}
