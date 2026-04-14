#include "include/sys/linux/LinuxCap.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

static QString cachedPkexecPath;
static QString cachedSudoPath;
static QString cachedHostSpawnPath;
static bool isFlatpak = false;

static void Linux_Init_Paths() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    isFlatpak = QFile::exists("/.flatpak-info");
    if (isFlatpak) {
        cachedHostSpawnPath = QStandardPaths::findExecutable("host-spawn");
        if (cachedHostSpawnPath.isEmpty()) {
            cachedHostSpawnPath = QStandardPaths::findExecutable("host-spawn", {"/app/bin", "/usr/bin"});
        }
    }
}

bool Linux_HavePkexec() {
    Linux_Init_Paths();
    static int cachedResult = -1; // -1: unknown, 0: no, 1: yes
    if (cachedResult != -1) return cachedResult == 1;

    QString pkexecPath;
    if (isFlatpak && !cachedHostSpawnPath.isEmpty()) {
        // In Flatpak we check for pkexec ON THE HOST via host-spawn
        QProcess p;
        p.setProgram(cachedHostSpawnPath);
        p.setArguments({"which", "pkexec"});
        p.start();
        if (p.waitForFinished(1000) && p.exitCode() == 0) {
            pkexecPath = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }
    } else {
        pkexecPath = QStandardPaths::findExecutable("pkexec");
        if (pkexecPath.isEmpty()) {
            pkexecPath = QStandardPaths::findExecutable("pkexec", {"/usr/bin", "/bin", "/usr/sbin", "/sbin"});
        }
    }
    
    if (pkexecPath.isEmpty()) {
        cachedResult = 0;
        return false;
    }

    cachedPkexecPath = pkexecPath;
    cachedResult = 1;
    return true;
}

bool Linux_HaveSudo() {
    Linux_Init_Paths();
    static int cachedResult = -1;
    if (cachedResult != -1) return cachedResult == 1;

    QString sudoPath;
    if (isFlatpak && !cachedHostSpawnPath.isEmpty()) {
        QProcess p;
        p.setProgram(cachedHostSpawnPath);
        p.setArguments({"which", "sudo"});
        p.start();
        if (p.waitForFinished(1000) && p.exitCode() == 0) {
            sudoPath = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }
    } else {
        sudoPath = QStandardPaths::findExecutable("sudo");
        if (sudoPath.isEmpty()) {
            sudoPath = QStandardPaths::findExecutable("sudo", {"/usr/bin", "/bin", "/usr/sbin", "/sbin"});
        }
    }
    
    if (sudoPath.isEmpty()) {
        cachedResult = 0;
        return false;
    }

    cachedSudoPath = sudoPath;
    cachedResult = 1;
    return true;
}

int Linux_Run_Command(const QString &commandName, const QString &args) {
    Linux_Init_Paths();
    QString fullCommand = Linux_FindCapProgsExec(commandName);
    QString command;
    
    QString elevationCmd;
    if (Linux_HavePkexec()) {
        elevationCmd = cachedPkexecPath;
    } else if (Linux_HaveSudo()) {
        elevationCmd = cachedSudoPath + " -n";
    } else {
        return 32512;
    }

    if (isFlatpak && !cachedHostSpawnPath.isEmpty()) {
        command = QString("%1 %2 %3 %4").arg(cachedHostSpawnPath, elevationCmd, fullCommand, args);
    } else {
        command = QString("%1 %2 %3").arg(elevationCmd, fullCommand, args);
    }

    return system(command.toStdString().c_str());
}

int Linux_Run_Privileged_Shell(const QString &shellCmd) {
    Linux_Init_Paths();
    QString command;
    
    // sh is usually at the same place on host and in flatpak, but let's be safe.
    QString shellPath = "/bin/sh"; 

    QString elevationCmd;
    if (Linux_HavePkexec()) {
        elevationCmd = cachedPkexecPath;
    } else if (Linux_HaveSudo()) {
        elevationCmd = cachedSudoPath + " -n";
    } else {
        return 32512;
    }

    if (isFlatpak && !cachedHostSpawnPath.isEmpty()) {
        command = QString("%1 %2 %3 -c '%4'").arg(cachedHostSpawnPath, elevationCmd, shellPath, QString(shellCmd).replace("'", "'\\''"));
    } else {
        command = QString("%1 %3 -c '%4'").arg(elevationCmd, shellPath, QString(shellCmd).replace("'", "'\\''"));
    }

    return system(command.toStdString().c_str());
}

QString Linux_FindCapProgsExec(const QString &name) {
    Linux_Init_Paths();
    QString exec;
    
    if (isFlatpak && !cachedHostSpawnPath.isEmpty()) {
        // Try to find the executable ON THE HOST
        QProcess p;
        p.setProgram(cachedHostSpawnPath);
        p.setArguments({"which", name});
        p.start();
        if (p.waitForFinished(1000) && p.exitCode() == 0) {
            exec = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }
    } 
    
    if (exec.isEmpty()) {
        exec = QStandardPaths::findExecutable(name);
        if (exec.isEmpty())
            exec = QStandardPaths::findExecutable(name, {"/usr/sbin", "/sbin", "/usr/bin", "/bin"});
    }

    return exec.isEmpty() ? name : exec;
}

bool Linux_IsPathNosuid(const QString &path) {
    // Inside Flatpak, mountinfo is for the sandbox. We should check the host if possible,
    // but usually /var/home is what matters.
    QFile f("/proc/self/mountinfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty()) canonical = path;

    QString bestMount;
    QString bestOpts;

    while (!f.atEnd()) {
        QByteArray lineBytes = f.readLine();
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        const QStringList fields = line.split(' ', Qt::SkipEmptyParts);
        if (fields.size() < 6) continue;
        const QString mountPoint = fields.at(4);
        const QString opts = fields.at(5);

        bool matches = (canonical == mountPoint) ||
                       (mountPoint == "/") ||
                       canonical.startsWith(mountPoint + "/");
        if (!matches) continue;

        if (mountPoint.length() >= bestMount.length()) {
            bestMount = mountPoint;
            bestOpts = opts;
        }
    }

    if (bestMount.isEmpty()) return false;
    const QStringList parts = bestOpts.split(',', Qt::SkipEmptyParts);
    return parts.contains("nosuid");
}

bool Linux_FileHasCapNetAdmin(const QString &path) {
    QString getcap = Linux_FindCapProgsExec("getcap");
    if (getcap.isEmpty()) return false;

    QProcess p;
    if (isFlatpak && !cachedHostSpawnPath.isEmpty() && getcap.startsWith("/")) {
        p.setProgram(cachedHostSpawnPath);
        p.setArguments({getcap, path});
    } else {
        p.setProgram(getcap);
        p.setArguments({path});
    }
    
    p.start();
    if (!p.waitForFinished(1500)) {
        p.kill();
        return false;
    }
    if (p.exitCode() != 0) return false;
    const QString output = QString::fromUtf8(p.readAllStandardOutput());
    return output.contains("cap_net_admin", Qt::CaseInsensitive);
}
