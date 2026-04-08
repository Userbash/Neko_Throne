// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/sys/platform/PlatformTray.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QSystemTrayIcon>

#ifdef Q_OS_LINUX
#include <QtDBus>
#endif

PlatformTray::PlatformTray(QObject *parent)
    : QObject(parent)
{
    setupPlatformBackend();
}

PlatformTray::~PlatformTray() {
    hide();
}

void PlatformTray::setupPlatformBackend() {
#ifdef Q_OS_LINUX
    // Prefer StatusNotifierItem on Wayland (QSystemTrayIcon is unreliable).
    // Qt 6 on Wayland auto-uses SNI if libdbusmenu-qt is available and the
    // XDG StatusNotifierWatcher service is registered.
    // We check whether the service exists on D-Bus.
    QDBusConnection session = QDBusConnection::sessionBus();
    if (session.isConnected()) {
        QDBusInterface watcher(QStringLiteral("org.kde.StatusNotifierWatcher"),
                               QStringLiteral("/StatusNotifierWatcher"),
                               QStringLiteral("org.kde.StatusNotifierWatcher"),
                               session);
        m_useSNI = watcher.isValid();
    }
    // Whether SNI is used or not, QSystemTrayIcon on Qt 6.5+ handles it internally
    // if compiled with D-Bus support. We always create one.
#endif

    m_trayIcon = new QSystemTrayIcon(this);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                switch (reason) {
                case QSystemTrayIcon::Trigger:
                    emit activated();
                    break;
                case QSystemTrayIcon::MiddleClick:
                    emit middleClicked();
                    break;
                case QSystemTrayIcon::DoubleClick:
                    emit doubleClicked();
                    break;
                default:
                    break;
                }
            });
}

void PlatformTray::setIcon(const QIcon &icon) {
    if (m_trayIcon)
        m_trayIcon->setIcon(icon);
}

void PlatformTray::setToolTip(const QString &tip) {
    if (m_trayIcon)
        m_trayIcon->setToolTip(tip);
}

void PlatformTray::setContextMenu(QMenu *menu) {
    m_menu = menu;
    if (m_trayIcon)
        m_trayIcon->setContextMenu(menu);
}

void PlatformTray::show() {
    if (m_trayIcon)
        m_trayIcon->show();
}

void PlatformTray::hide() {
    if (m_trayIcon)
        m_trayIcon->hide();
}

void PlatformTray::showMessage(const QString &title, const QString &message,
                               const QIcon &icon, int msecs) {
    if (m_trayIcon)
        m_trayIcon->showMessage(title, message, icon, msecs);
}

bool PlatformTray::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}
