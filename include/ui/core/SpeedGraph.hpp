// SPDX-License-Identifier: GPL-2.0-or-later
// SpeedGraph — 60 FPS traffic monitor with fixed-capacity ring buffer.
// Memory usage is O(1) regardless of uptime.
// Renders via QPainter (backed by RHI compositing through QWidget pipeline).

#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <array>
#include <cstddef>
#include <cstdint>

class SpeedGraph : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int refreshRate READ refreshRate WRITE setRefreshRate)

public:
    explicit SpeedGraph(QWidget *parent = nullptr, int bufferSeconds = 120);
    ~SpeedGraph() override = default;

    // Push a single data point (bytes/sec). Thread-safe via queued signal.
    void addDataPoint(qint64 downloadSpeed, qint64 uploadSpeed);

    // Clear all stored data.
    void clear();

    int refreshRate() const { return m_refreshMs; }
    void setRefreshRate(int ms);

signals:
    void peakSpeedChanged(qint64 download, qint64 upload);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // ---- Ring buffer ----
    struct DataPoint {
        qint64 download = 0;
        qint64 upload = 0;
    };

    static constexpr std::size_t MAX_POINTS = 7200; // 2h @ 1 sample/sec
    std::array<DataPoint, MAX_POINTS> m_buffer{};
    std::size_t m_head = 0;   // next write position
    std::size_t m_count = 0;  // number of valid points

    void push(const DataPoint &pt);
    DataPoint at(std::size_t logicalIndex) const;

    // ---- Rendering ----
    int m_refreshMs = 16; // ~60 FPS
    QTimer m_repaintTimer;

    qint64 m_peakDownload = 0;
    qint64 m_peakUpload = 0;

    QColor m_downloadColor{0x3D, 0x9B, 0xFF};   // #3D9BFF
    QColor m_uploadColor{0xFF, 0x6B, 0x6B};      // #FF6B6B
    QColor m_gridColor{80, 80, 80, 60};

    void drawGrid(QPainter &p, const QRectF &area);
    void drawGraph(QPainter &p, const QRectF &area, bool isUpload);
    void drawLegend(QPainter &p, const QRectF &area);

    static QString formatSpeed(qint64 bytesPerSec);
};
