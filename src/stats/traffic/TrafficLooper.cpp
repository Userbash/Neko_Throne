#include "include/stats/traffic/TrafficLooper.hpp"

#include "include/api/RPC.h"
#include "include/ui/mainwindow_interface.h"

#include <QThread>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace Stats {

    // Thread-safe singleton - no memory leak
    TrafficLooper* GetTrafficLooper() {
        static TrafficLooper instance;
        return &instance;
    }

    void TrafficLooper::UpdateAll() {
        if (Configs::dataStore->disable_traffic_stats) {
            return;
        }

        // Guard: called from profile_stop even when no profile has ever started;
        // proxy/direct are nullptr until profile_start sets them.
        if (!proxy || !direct) {
            return;
        }

        auto resp = API::defaultClient->QueryStats();
        if (proxy) {
            proxy->uplink_rate = 0;
            proxy->downlink_rate = 0;
        }

        int proxyUp = 0, proxyDown = 0;

        for (const auto &item: this->items) {
            if (!item || !resp.ups.contains(item->tag)) continue;
            auto now = elapsedTimer.elapsed();
            auto interval = now - item->last_update;
            item->last_update = now;
            if (interval <= 0) continue;
            auto up = resp.ups.at(item->tag);
            auto down = resp.downs.at(item->tag);
            if (item->tag == "proxy")
            {
                proxyUp = up;
                proxyDown = down;
            }
            item->uplink += up;
            item->downlink += down;
            item->uplink_rate = static_cast<double>(up) * 1000.0 / static_cast<double>(interval);
            item->downlink_rate = static_cast<double>(down) * 1000.0 / static_cast<double>(interval);
            if (item->ignoreForRate) continue;
            if (item->tag == "direct")
            {
                if (direct) {
                    direct->uplink_rate = item->uplink_rate;
                    direct->downlink_rate = item->downlink_rate;
                }
            } else
            {
                if (proxy) {
                    proxy->uplink_rate += item->uplink_rate;
                    proxy->downlink_rate += item->downlink_rate;
                }
            }
        }
        if (isChain)
        {
            for (const auto &item: this->items)
            {
                if (item && item->isChainTail)
                {
                    item->uplink += proxyUp;
                    item->downlink += proxyDown;
                }
            }
        }
    }

    void TrafficLooper::Loop() {
        elapsedTimer.start();
        while (!m_stop.load()) {
            QThread::msleep(1000); // refresh every one second

            if (Configs::dataStore->disable_traffic_stats) {
                continue;
            }

            // profile start and stop
            if (!loop_enabled) {
                // 停止
                if (looping) {
                    looping = false;
                    runOnUiThread([=] {
                        auto m = GetMainWindow();
                        if (m != nullptr) {
                            m->refresh_status("STOP");
                        }
                    });
                }
                runOnUiThread([=]
                {
                   auto m = GetMainWindow();
                   if (m != nullptr) {
                       m->update_traffic_graph(0, 0, 0, 0);
                   }
                });
                continue;
            } else {
                // 开始
                if (!looping) {
                    looping = true;
                }
            }

            // do update
            loop_mutex.lock();

            UpdateAll();

            // Capture data for UI while holding the lock
            auto itemsCopy = this->items;
            auto proxyCopy = this->proxy;
            auto directCopy = this->direct;

            loop_mutex.unlock();

            // post to UI using thread-safe copies
            runOnUiThread([=, this] {
                // Check if application is already quitting or main window is gone
                if (QCoreApplication::closingDown() || !QApplication::instance()) return;
                auto m = GetMainWindow();
                if (m == nullptr) return;

                if (proxyCopy != nullptr && directCopy != nullptr) {
                    m->refresh_status(QObject::tr("Proxy: %1\nDirect: %2").arg(proxyCopy->DisplaySpeed(), directCopy->DisplaySpeed()));
                    m->update_traffic_graph(proxyCopy->downlink_rate, proxyCopy->uplink_rate, directCopy->downlink_rate, directCopy->uplink_rate);
                }
                for (const auto &item: itemsCopy) {
                    if (!item || item->id < 0) continue;
                    m->refresh_proxy_list(item->id);
                }
            });
        }
        looping = false;
    }

    void TrafficLooper::stop() {
        m_stop.store(true);
        // We don't wait for finished here since it's a separate thread usually,
        // but it will exit at next iteration.
    }

    void TrafficLooper::waitForStop() {
        if (m_future.isRunning()) {
            m_future.waitForFinished();
        }
    }

} // namespace Stats
