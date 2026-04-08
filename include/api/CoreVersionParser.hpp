// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// include/api/CoreVersionParser.hpp — Asynchronous sing-box / Xray-core
// version detection and operational status monitor.
//
// Runs QProcess off the main thread, emits parsed results as Qt signals.
// Never blocks the GUI event loop.
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <QObject>
#include <QString>

struct CoreVersionInfo {
    QString singboxVersion;    // e.g. "1.12.13"
    QString xrayVersion;       // e.g. "1.251208.0"
    QString singboxStatus;     // "running" | "stopped" | "error"
    QString xrayStatus;        // "running" | "stopped" | "error"
    bool singboxAvailable = false;
    bool xrayAvailable = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// CoreVersionParser
// ═══════════════════════════════════════════════════════════════════════════════
// Usage:
//   auto *parser = CoreVersionParser::instance();
//   connect(parser, &CoreVersionParser::versionParsed, this, &MyWidget::onVersion);
//   parser->requestVersions();
//   parser->startPeriodicCheck(5000); // every 5s
//
class CoreVersionParser : public QObject {
    Q_OBJECT

public:
    static CoreVersionParser *instance();

    // Fire an asynchronous version query via QProcess.
    // The core binary is located via Configs::FindCoreRealPath().
    void requestVersions();

    // Start a periodic poll (ms interval) — safe, timer runs on this object's thread.
    void startPeriodicCheck(int intervalMs = 5000);
    void stopPeriodicCheck();

    // Last cached result (thread-safe read).
    CoreVersionInfo cachedInfo() const;

signals:
    // Emitted on the main thread after each successful parse.
    void versionParsed(const CoreVersionInfo &info);

    // Emitted if the core binary cannot be found or fails to execute.
    void parseError(const QString &errorMessage);

private:
    explicit CoreVersionParser(QObject *parent = nullptr);
    ~CoreVersionParser() override = default;

    Q_DISABLE_COPY_MOVE(CoreVersionParser)

    class Impl;
    Impl *d;
};
