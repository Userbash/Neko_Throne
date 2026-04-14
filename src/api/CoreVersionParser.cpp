#include <mutex>
// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// src/api/CoreVersionParser.cpp — Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "include/api/CoreVersionParser.hpp"
#include "include/api/RPC.h"
#include "include/global/Configs.hpp"

#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QThreadPool>
#include <QTimer>
#include <QPointer>
#include <QtConcurrent>
#include "3rdparty/qscopeguard.h"

// ─── Private implementation ──────────────────────────────────────────────────
/**
 * @brief Private data container for CoreVersionParser (PIMPL-ish).
 * Stores the cached version info and the timer for periodic checks.
 */
class CoreVersionParser::Impl {
public:
    mutable QMutex mutex;      ///< Guards access to cached data
    CoreVersionInfo cached;    ///< Last successfully parsed version information
    QTimer *periodicTimer = nullptr; ///< Timer for background polling
    int gRPCFailCount = 0;     ///< Tracker for consecutive gRPC failures
    std::atomic<bool> isRunning{false}; ///< Flag to prevent concurrent checks
};

// ─── Singleton ───────────────────────────────────────────────────────────────
CoreVersionParser *CoreVersionParser::instance() {
    static CoreVersionParser s_instance;
    return &s_instance;
}

CoreVersionParser::CoreVersionParser(QObject *parent)
    : QObject(parent), d(new Impl) {}

CoreVersionInfo CoreVersionParser::cachedInfo() const {
    QMutexLocker lk(&d->mutex);
    return d->cached;
}

// ─── Async version query ─────────────────────────────────────────────────────
/**
 * @brief Spawns the core binary with "--version" in a separate thread.
 * Uses regex to extract version strings for both sing-box and Xray-core.
 * Emits versionParsed() on the main thread when finished.
 */
void CoreVersionParser::requestVersions() {
    // Prevent overlapping checks
    if (d->isRunning.exchange(true)) {
        return;
    }

    QPointer<CoreVersionParser> safeThis(this);

    (void) QtConcurrent::run([this, safeThis] {
        CoreVersionInfo info;

        auto cleanup = qScopeGuard([this] {
            d->isRunning.store(false);
        });

        // Resolve absolute path to the core binary (NekoCore)
        auto corePath = Configs::FindCoreRealPath();
        if (corePath.isEmpty()) {
            if (safeThis) {
                QMetaObject::invokeMethod(safeThis, [this] {
                    emit parseError(QStringLiteral("Core binary not found on disk"));
                });
            }
            return;
        }

        // ── Execute Core Binary ──────────────────────────────────────────
        {
            QProcess proc;
            proc.setProgram(corePath);
            proc.setArguments({QStringLiteral("--version")});
            proc.start(QIODevice::ReadOnly);
            
            // Wait up to 5 seconds. Core binaries normally return versions instantly.
            if (proc.waitForFinished(5000)) {
                auto output = QString::fromUtf8(proc.readAllStandardOutput());
                
                // 1. Regex for sing-box (Matches: "sing-box: v1.13.2" or "sing-box version 1.13.2" or "v1.13.2")
                static const QRegularExpression rxSingbox(
                    QStringLiteral(R"((?:sing-box[:\s]+)?(?:version\s+)?v?([\d.]+(?:-\w+)?))")
                );
                auto m = rxSingbox.match(output);
                if (m.hasMatch()) {
                    info.singboxVersion = m.captured(1);
                    info.singboxAvailable = true;
                    info.singboxStatus = QStringLiteral("stopped");
                }

                // 2. Regex for Xray (Matches: "Xray-core: 1.25.1" or "Xray 1.25.1" or "1.25.1")
                static const QRegularExpression rxXray(
                    QStringLiteral(R"((?:Xray(?:-core)?[:\s]+)?v?([\d.]+))")
                );
                auto mx = rxXray.match(output);
                if (mx.hasMatch()) {
                    info.xrayVersion = mx.captured(1);
                    info.xrayAvailable = true;
                    info.xrayStatus = QStringLiteral("stopped");
                }
            } else {
                info.singboxStatus = QStringLiteral("error");
                info.xrayStatus = QStringLiteral("error");
                MW_show_log(QString("[CoreManager] Version check timed out for %1").arg(corePath));
            }
        }

        // ── Detect Operational Status ────────────────────────────────────
        // Check if the proxy engine is currently active according to our internal state.
        if (Configs::dataStore && Configs::dataStore->core_running) {
            if (info.singboxAvailable)
                info.singboxStatus = QStringLiteral("running");
            if (info.xrayAvailable)
                info.xrayStatus = QStringLiteral("running");

            // ── gRPC Heartbeat Check ─────────────────────────────────────
            // Perform a lightweight gRPC call to ensure the core is responsive.
            bool rpcOk = false;
            if (API::defaultClient) {
                API::defaultClient->IsPrivileged(&rpcOk);
            }

            if (!rpcOk) {
                if (++d->gRPCFailCount >= 3) {
                    MW_show_log(QStringLiteral("[Fatal] gRPC heartbeat failed 3 times. Core might be stuck."));
                    if (safeThis) {
                        QMetaObject::invokeMethod(safeThis, [this] {
                            emit heartbeatFailed();
                        });
                    }
                }
            } else {
                d->gRPCFailCount = 0; // reset on success
            }
        } else {
            d->gRPCFailCount = 0; // core not running, no heartbeat needed
        }

        // ── Update Cache & Notify GUI ────────────────────────────────────
        {
            QMutexLocker lk(&d->mutex);
            d->cached = info;
        }

        // Emit signal on main thread for UI consumption
        if (safeThis) {
            QMetaObject::invokeMethod(safeThis, [this, info] {
                emit versionParsed(info);
            });
        }
    });
}

// ─── Periodic polling ────────────────────────────────────────────────────────
void CoreVersionParser::startPeriodicCheck(int intervalMs) {
    if (!d->periodicTimer) {
        d->periodicTimer = new QTimer(this);
        connect(d->periodicTimer, &QTimer::timeout, this, &CoreVersionParser::requestVersions);
    }
    d->periodicTimer->start(intervalMs);
    requestVersions(); // Immediate first-run query
}

void CoreVersionParser::stopPeriodicCheck() {
    if (d->periodicTimer)
        d->periodicTimer->stop();
}
