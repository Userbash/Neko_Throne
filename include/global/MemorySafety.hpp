// SPDX-License-Identifier: GPL-2.0-or-later
// ═══════════════════════════════════════════════════════════════════════════════
// include/global/MemorySafety.hpp — Memory management best practices and
// utilities for preventing RAM bloat during long proxy sessions.
//
// Key concerns addressed:
//   1. QObject lifecycle management (parent-child ownership)
//   2. Smart pointer patterns for data buffers in I/O pipelines
//   3. Bounded buffer pools for traffic statistics
//   4. RAII guards for RPC client connections
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <memory>
#include <functional>

namespace MemSafety {

// ═══════════════════════════════════════════════════════════════════════════════
// 1. ScopedRPCClient — RAII guard for gRPC channel connections
// ═══════════════════════════════════════════════════════════════════════════════
// Ensures the RPC connection is closed even if an exception is thrown or
// the calling function returns early.
//
// Usage:
//   auto guard = ScopedRPCClient(make_client());
//   guard->CallMethod("Start", ...);
//   // connection auto-closes when 'guard' goes out of scope
//
template <typename Client>
class ScopedRPCClient {
public:
    explicit ScopedRPCClient(std::unique_ptr<Client> client)
        : m_client(std::move(client)) {}

    ~ScopedRPCClient() = default;

    // Move-only
    ScopedRPCClient(ScopedRPCClient &&) = default;
    ScopedRPCClient &operator=(ScopedRPCClient &&) = default;

    ScopedRPCClient(const ScopedRPCClient &) = delete;
    ScopedRPCClient &operator=(const ScopedRPCClient &) = delete;

    Client *operator->() { return m_client.get(); }
    Client &operator*() { return *m_client; }
    explicit operator bool() const { return m_client != nullptr; }

private:
    std::unique_ptr<Client> m_client;
};

// ═══════════════════════════════════════════════════════════════════════════════
// 2. BoundedBuffer — Thread-safe, bounded FIFO for traffic data points
// ═══════════════════════════════════════════════════════════════════════════════
// Prevents unbounded growth of in-memory traffic samples during long uptimes.
// When capacity is reached, oldest entries are silently dropped.
//
// Usage:
//   BoundedBuffer<TrafficSample> buffer(10000); // max 10k samples
//   buffer.enqueue(sample);
//   auto latest = buffer.dequeueAll();
//
template <typename T>
class BoundedBuffer {
public:
    explicit BoundedBuffer(int capacity = 10000) : m_capacity(capacity) {}

    void enqueue(const T &item) {
        QMutexLocker lk(&m_mutex);
        if (m_queue.size() >= m_capacity) {
            m_queue.dequeue(); // drop oldest
        }
        m_queue.enqueue(item);
    }

    void enqueue(T &&item) {
        QMutexLocker lk(&m_mutex);
        if (m_queue.size() >= m_capacity) {
            m_queue.dequeue();
        }
        m_queue.enqueue(std::move(item));
    }

    QList<T> dequeueAll() {
        QMutexLocker lk(&m_mutex);
        QList<T> result;
        result.reserve(m_queue.size());
        while (!m_queue.isEmpty())
            result.append(m_queue.dequeue());
        return result;
    }

    int size() const {
        QMutexLocker lk(&m_mutex);
        return m_queue.size();
    }

    void clear() {
        QMutexLocker lk(&m_mutex);
        m_queue.clear();
    }

private:
    mutable QMutex m_mutex;
    QQueue<T> m_queue;
    int m_capacity;
};

// ═══════════════════════════════════════════════════════════════════════════════
// 3. SafeQObjectDeleter — Deferred QObject deletion on correct thread
// ═══════════════════════════════════════════════════════════════════════════════
// QObjects must be deleted on the thread that owns them.
// This helper ensures deleteLater() is always used.
//
// Usage:
//   std::unique_ptr<QProcess, SafeQObjectDeleter> proc(new QProcess);
//
struct SafeQObjectDeleter {
    void operator()(QObject *obj) const {
        if (obj) {
            if (obj->thread() == QThread::currentThread()) {
                delete obj;
            } else {
                obj->deleteLater();
            }
        }
    }
};

template <typename T>
using SafeQObjectPtr = std::unique_ptr<T, SafeQObjectDeleter>;

// ═══════════════════════════════════════════════════════════════════════════════
// 4. ScopeGuard — Generic RAII cleanup (C++20 style)
// ═══════════════════════════════════════════════════════════════════════════════
// For ad-hoc cleanup of raw buffers, file handles, etc.
//
// Usage:
//   char *buf = new char[4096];
//   auto guard = ScopeGuard([buf] { delete[] buf; });
//
template <typename Func>
class ScopeGuard {
public:
    explicit ScopeGuard(Func &&f) : m_func(std::move(f)), m_active(true) {}
    ~ScopeGuard() { if (m_active) m_func(); }

    ScopeGuard(ScopeGuard &&other) noexcept
        : m_func(std::move(other.m_func)), m_active(other.m_active) {
        other.dismiss();
    }

    void dismiss() { m_active = false; }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
    ScopeGuard &operator=(ScopeGuard &&) = delete;

private:
    Func m_func;
    bool m_active;
};

template <typename Func>
ScopeGuard<Func> makeScopeGuard(Func &&f) {
    return ScopeGuard<Func>(std::forward<Func>(f));
}

} // namespace MemSafety

// ═══════════════════════════════════════════════════════════════════════════════
// MEMORY MANAGEMENT CHECKLIST (for code review reference)
// ═══════════════════════════════════════════════════════════════════════════════
//
// QObject Lifecycle:
//   [x] All QObjects created with 'new' have a parent, OR are tracked by
//       a smart pointer with SafeQObjectDeleter.
//   [x] QTimers stopped in destructor or handled via parent ownership.
//   [x] QProcess instances cleaned up via deleteLater() from finished signal.
//   [x] Signals disconnected before deleting senders in non-parent scenarios.
//
// Smart Pointers:
//   [x] std::shared_ptr for ProxyEntity, TrafficData (shared across threads).
//   [x] std::unique_ptr for single-owner resources (RPC clients, processes).
//   [x] No raw 'new' without corresponding RAII wrapper.
//
// I/O Buffers:
//   [x] BoundedBuffer used for all traffic sample queues.
//   [x] RPC response buffers are stack-allocated (protobuf value types).
//   [x] QByteArray used for raw data (implicit sharing + CoW).
//
// Thread Safety:
//   [x] QMutex guards all shared mutable state.
//   [x] std::atomic for flags and counters.
//   [x] QMetaObject::invokeMethod for cross-thread signal delivery.
//   [x] No std::thread — only QThread, QThreadPool, QtConcurrent.
//
// Common Pitfalls Avoided:
//   [x] No QMutex locked during signal emission (deadlock risk).
//   [x] No blocking calls on the main thread.
//   [x] No capturing 'this' in lambdas outliving the object.
//   [x] DoneMu pattern uses QMutex pointer (not stack) for cross-thread usage
//       — BUT should migrate to QPromise/QFuture for cleaner lifecycle.
//
