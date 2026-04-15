#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/mainwindow.h"
#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#endif

#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QTcpServer>
#include <QFile>
#include <QTextStream>

namespace Configs_sys {

    static bool isPortBusy(int port) {
        QFile file("/proc/net/tcp");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QTextStream in(&file);
        QString hexPort = QString::number(port, 16).toUpper().rightJustified(4, '0');
        while (!in.atEnd()) {
            if (in.readLine().contains(":" + hexPort)) return true;
        }
        return false;
    }

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) {
        program = core_path;
        arguments = args;

        connect(this, &QProcess::readyReadStandardOutput, this, [this]() {
            auto log = readAllStandardOutput();
            if (m_state == CoreLifecycleState::Starting) {
                if (log.contains("Core listening at")) {
                    m_state = CoreLifecycleState::Running;
                    Configs::dataStore->core_running = true;
                    MW_show_log("CoreStarted");
                }
            }
            MW_show_log(log);
        });
        connect(this, &QProcess::readyReadStandardError, this, [this]() {
            MW_show_log(readAllStandardError());
        });
    }

    void CoreProcess::Start() {
        if (started) return;
        started = true;
        m_state = CoreLifecycleState::Starting;
        logCounter.storeRelaxed(0);
        setEnvironment(QProcessEnvironment::systemEnvironment().toStringList());

#ifdef Q_OS_LINUX
        int rpcPort = Configs::dataStore->core_port;
        if (rpcPort > 0 && isPortBusy(rpcPort)) {
            MW_show_log("IPC Watchdog: RPC port busy, skipping start");
            m_state = CoreLifecycleState::Failed;
            return;
        }

        if (Configs::dataStore->spmode_vpn && !Configs::IsAdmin() && !Linux_FileHasCapNetAdmin(program)) {
            MW_show_log(tr("Core lacks CAP_NET_ADMIN. Attempting pkexec..."));
            QString pkexec = Linux_FindCapProgsExec("pkexec");
            if (!pkexec.isEmpty()) {
                start(pkexec, QStringList() << program << arguments);
                return;
            }
        }
#endif
        start(program, arguments);
    }

    void CoreProcess::Restart() {
        restarting = true;
        m_state = CoreLifecycleState::Restarting;
        Kill();
        started = false;
        Start();
        QTimer::singleShot(200, this, [this] { restarting = false; });
    }

    void CoreProcess::Kill() {
        if (state() != QProcess::Running) return;
        m_state = CoreLifecycleState::Stopping;
        terminate();
        if (!waitForFinished(1500)) {
            kill();
            waitForFinished(500);
        }
    }
    
    CoreProcess::~CoreProcess() {
        Kill();
    }
} // namespace Configs_sys
