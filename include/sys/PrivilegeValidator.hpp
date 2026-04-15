#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

class PrivilegeValidator {
public:
    static bool isImmutableOS() {
        return QFile::exists("/run/ostree-booted");
    }

    static bool validateMountFlags(const QString& path) {
        QFile mounts("/proc/mounts");
        if (!mounts.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        
        QTextStream in(&mounts);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.contains(path)) {
                if (line.contains("nosuid") || line.contains("noexec")) return false;
            }
        }
        return true;
    }

    static QString getSafeCorePath() {
        if (isImmutableOS()) return "USE_DIRECT_PKEXEC";

        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/privileged";
        QDir().mkpath(cachePath);
        
        if (validateMountFlags(cachePath)) {
            return cachePath + "/NekoCore";
        }

        return "USE_DIRECT_PKEXEC";
    }
};
