#pragma once

#include <QString>
#include <QList>
#include <QMutex>
#include <QElapsedTimer>
#include <QFuture>
#include <atomic>

#include "TrafficData.hpp"

namespace Stats {
    class TrafficLooper {
    public:
        std::atomic<bool> loop_enabled = false;
        std::atomic<bool> looping = false;
        std::atomic<bool> m_stop = false;
        QFuture<void> m_future;
        QMutex loop_mutex;
        QElapsedTimer elapsedTimer;

        QList<std::shared_ptr<TrafficData>> items;
        bool isChain;
        std::shared_ptr<TrafficData> proxy;
        std::shared_ptr<TrafficData> direct;

        void UpdateAll();

        void Loop();
        void stop();
        void waitForStop();
    };

    // Thread-safe singleton accessor
    TrafficLooper* GetTrafficLooper();
    
    // Legacy compatibility
    #define trafficLooper GetTrafficLooper()
} // namespace Stats
