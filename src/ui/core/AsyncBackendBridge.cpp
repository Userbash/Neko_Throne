// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/ui/core/AsyncBackendBridge.hpp"
#include "include/api/RPC.h"
#include "include/global/Configs.hpp"

#include <QThreadPool>
#include <QtConcurrent>

AsyncBackendBridge *AsyncBackendBridge::instance() {
    static AsyncBackendBridge s_instance;
    return &s_instance;
}

AsyncBackendBridge::AsyncBackendBridge(QObject *parent)
    : QObject(parent) {}

// Helper: run a lambda on QThreadPool, invoke `onResult` on main thread via signal.
template <typename Func>
void AsyncBackendBridge::runAsync(Func &&func) {
    QtConcurrent::run(QThreadPool::globalInstance(), std::forward<Func>(func));
}

// ---------------------------------------------------------------------------
// Core lifecycle
// ---------------------------------------------------------------------------

void AsyncBackendBridge::startCore(const libcore::LoadConfigReq &req) {
    auto reqCopy = req; // capture by value for thread safety
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this, reqCopy]() {
        bool rpcOK = false;
        QString err = API::defaultClient->Start(&rpcOK, reqCopy);
        if (!rpcOK) { emit backendError(QStringLiteral("startCore"), QStringLiteral("RPC connection failed")); return; }
        if (err.isEmpty())
            emit coreStarted();
        else
            emit coreStartFailed(err);
    });
}

void AsyncBackendBridge::stopCore() {
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this]() {
        bool rpcOK = false;
        QString err = API::defaultClient->Stop(&rpcOK);
        if (!rpcOK) { emit backendError(QStringLiteral("stopCore"), QStringLiteral("RPC connection failed")); return; }
        if (err.isEmpty())
            emit coreStopped();
        else
            emit backendError(QStringLiteral("stopCore"), err);
    });
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

void AsyncBackendBridge::queryStats() {
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this]() {
        auto resp = API::defaultClient->QueryStats();
        emit statsReady(resp);
    });
}

void AsyncBackendBridge::queryConnections() {
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this]() {
        auto resp = API::defaultClient->ListConnections();
        emit connectionsReady(resp);
    });
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void AsyncBackendBridge::runLatencyTest(const libcore::TestReq &req) {
    auto reqCopy = req;
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this, reqCopy]() {
        bool rpcOK = false;
        auto resp = API::defaultClient->Test(&rpcOK, reqCopy);
        if (rpcOK) emit latencyTestDone(resp);
    });
}

void AsyncBackendBridge::stopTests() {
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this]() {
        bool rpcOK = false;
        API::defaultClient->StopTests(&rpcOK);
    });
}

void AsyncBackendBridge::runSpeedTest(const libcore::SpeedTestRequest &req) {
    auto reqCopy = req;
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this, reqCopy]() {
        bool rpcOK = false;
        auto resp = API::defaultClient->SpeedTest(&rpcOK, reqCopy);
        if (rpcOK) emit speedTestDone(resp);
    });
}

void AsyncBackendBridge::querySpeedTestResults() {
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this]() {
        bool rpcOK = false;
        auto resp = API::defaultClient->QueryCurrentSpeedTests(&rpcOK);
        if (rpcOK) emit speedTestProgress(resp);
    });
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void AsyncBackendBridge::checkConfig(const QString &config) {
    QString cfgCopy = config;
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this, cfgCopy]() {
        bool rpcOK = false;
        QString err = API::defaultClient->CheckConfig(&rpcOK, cfgCopy);
        if (!rpcOK) { emit backendError(QStringLiteral("checkConfig"), QStringLiteral("RPC connection failed")); return; }
        emit configCheckResult(err);
    });
}

// ---------------------------------------------------------------------------
// DNS
// ---------------------------------------------------------------------------

void AsyncBackendBridge::setSystemDNS(bool clear) {
    (void) QtConcurrent::run(QThreadPool::globalInstance(), [this, clear]() {
        bool rpcOK = false;
        QString err = API::defaultClient->SetSystemDNS(&rpcOK, clear);
        if (!rpcOK) { emit backendError(QStringLiteral("setSystemDNS"), QStringLiteral("RPC connection failed")); return; }
        emit systemDNSSet(err);
    });
}
