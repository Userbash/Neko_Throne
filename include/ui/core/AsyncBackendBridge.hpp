// SPDX-License-Identifier: GPL-2.0-or-later
// AsyncBackendBridge — Zero-freeze bridge between GUI and backend (sing-box core via RPC).
// All RPC calls are dispatched to QThreadPool; results delivered via signals on the main thread.

#pragma once

#include <QObject>
#include <QFuture>
#include <QPromise>
#include <functional>

#ifndef Q_MOC_RUN
#include <libcore.pb.h>
#endif

// Forward — the actual RPC::Client from include/api/RPC.h
namespace API { class Client; }

class AsyncBackendBridge : public QObject {
    Q_OBJECT

public:
    static AsyncBackendBridge *instance();

    // ── Core lifecycle ──
    void startCore(const libcore::LoadConfigReq &req);
    void stopCore();

    // ── Statistics (non-blocking) ──
    void queryStats();
    void queryConnections();

    // ── Tests ──
    void runLatencyTest(const libcore::TestReq &req);
    void stopTests();
    void runSpeedTest(const libcore::SpeedTestRequest &req);
    void querySpeedTestResults();

    // ── Config validation ──
    void checkConfig(const QString &config);

    // ── DNS ──
    void setSystemDNS(bool clear);

    // ── Generic: run any blocking lambda off the main thread,
    //    deliver result via a callback on the GUI thread. ──
    template <typename Func>
    void runAsync(Func &&func);

signals:
    // Core lifecycle
    void coreStarted();                             // successful start
    void coreStartFailed(const QString &error);
    void coreStopped();

    // Stats
    void statsReady(const libcore::QueryStatsResp &resp);
    void connectionsReady(const libcore::ListConnectionsResp &resp);

    // Tests
    void latencyTestDone(const libcore::TestResp &resp);
    void speedTestProgress(const libcore::QuerySpeedTestResponse &resp);
    void speedTestDone(const libcore::SpeedTestResponse &resp);

    // Config
    void configCheckResult(const QString &error); // empty → OK

    // DNS
    void systemDNSSet(const QString &error);

    // Generic error
    void backendError(const QString &context, const QString &message);

private:
    explicit AsyncBackendBridge(QObject *parent = nullptr);
    ~AsyncBackendBridge() override = default;

    Q_DISABLE_COPY_MOVE(AsyncBackendBridge)
};
