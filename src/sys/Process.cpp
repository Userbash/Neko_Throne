#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/mainwindow.h"
#include "include/dataStore/Database.hpp"
#include "include/sys/PrivilegeValidator.hpp"
#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#endif

#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>

namespace Configs_sys {

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) {
        program = core_path;
        arguments = args;

        connect(this, &QProcess::readyReadStandardOutput, this, [this]() {
            auto log = readAllStandardOutput();
            if (m_state == CoreLifecycleState::Starting && log.contains("Core listening at")) {
                m_state = CoreLifecycleState::Running;
                Configs::dataStore->core_running = true;
                MW_show_log("CoreStarted");
            }
            MW_show_log(log);
        });

        connect(this, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
            Configs::dataStore->core_running = false;
            m_state = CoreLifecycleState::Stopped;
            MW_show_log(QString("Core exited with code %1").arg(exitCode));
        });
    }

    void CoreProcess::Start() {
        // 1. IPC Watchdog: Clear port
        int rpcPort = Configs::dataStore->core_port;
        if (rpcPort > 0) QProcess::execute("fuser", {"-k", QString::number(rpcPort) + "/tcp"});

        if (state() == QProcess::Running) {
            terminate();
            if (!waitForFinished(2000)) kill();
        }

        // 2. Get Safe Path
        QString safePath = PrivilegeValidator::getSafeCorePath();
        bool useDirectPkexec = (safePath == "USE_DIRECT_PKEXEC");
        
        QString pathToRun = program;
        if (!useDirectPkexec) {
            QFile::remove(safePath);
            QFile::copy(program, safePath);
            QFile::setPermissions(safePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            pathToRun = safePath;
        }
        
        if (!QFileInfo(pathToRun).isExecutable()) {
            emit this->coreError(tr("Core binary not executable."));
            return;
        }

        started = true;
        m_state = CoreLifecycleState::Starting;
        setEnvironment(QProcessEnvironment::systemEnvironment().toStringList());

#ifdef Q_OS_LINUX
        bool needElevation = Configs::dataStore->spmode_vpn && !Configs::IsAdmin();
        if (needElevation) {
            QString pkexec = Linux_FindCapProgsExec("pkexec");
            if (pkexec.isEmpty()) {
                emit this->coreError(tr("TUN mode requires 'pkexec'."));
                return;
            }
            // Execute as privileged process
            this->start(pkexec, QStringList() << pathToRun << arguments);
            return;
        }
#endif
        this->start(pathToRun, arguments);
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
        if (state() == QProcess::Running) {
            terminate();
            if (!waitForFinished(1500)) kill();
        }
    }
    
    CoreProcess::~CoreProcess() {
        Kill();
    }
}
