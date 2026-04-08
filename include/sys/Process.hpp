#pragma once

#include <memory>
#include <QElapsedTimer>
#include <QProcess>

namespace Configs_sys {

    // ─── Explicit lifecycle states for the core process ───────────────────────
    // Replaces the implicit bool flags with a well-defined state machine.
    //
    //  stopped  ──start──►  starting  ──ready──►  running
    //     ▲                                          │
    //     └─────────────────────────────stop/crash───┘
    //                                                │
    //  failed  ◄──(startup error)──  stopping  ◄────┘
    //     │                              ▲
    //     └──────────retry──────────────►│ (via Restart)
    //
    enum class CoreLifecycleState {
        Stopped,     // Not running; initial state or after normal stop
        Starting,    // Process launched, waiting for "Core listening at" log line
        Running,     // Core acknowledged ready
        Stopping,    // Kill/terminate requested; awaiting exit
        Restarting,  // Kill → restart sequence in progress
        Failed,      // Startup failed (binary missing, port in use, etc.)
    };

    class CoreProcess : public QProcess
    {
    public:
        QString tag;
        QString program;
        QStringList arguments;

        ~CoreProcess();

        // start & kill is one time

        void Start();

        void Kill();

        CoreProcess(const QString &core_path, const QStringList &args);

        void Restart();

        // Current lifecycle state (read-only by external code)
        CoreLifecycleState lifecycleState() const { return m_state; }

        int start_profile_when_core_is_up = -1;

    private:
        bool show_stderr = false;
        bool failed_to_start = false;
        bool restarting = false;

        CoreLifecycleState m_state = CoreLifecycleState::Stopped;

        QElapsedTimer coreRestartTimer;

    protected:
        bool started = false;
        bool crashed = false;
    };

    inline QAtomicInt logCounter;
} // namespace Configs_sys
