// SPDX-License-Identifier: GPL-2.0-or-later
// PlatformTray — Modern system tray abstraction.
// On Linux/Wayland: uses StatusNotifierItem (SNI) via D-Bus for reliable indicator.
// On Windows/X11: wraps QSystemTrayIcon with show-on-activation fix.
// On all platforms: supports silent start (no window flash), thin wrapper with unified API.

#pragma once

#include <QObject>
#include <QIcon>
#include <QMenu>
#include <memory>

class QSystemTrayIcon;

#ifdef Q_OS_LINUX
class QDBusInterface;
#endif

class PlatformTray : public QObject {
    Q_OBJECT

public:
    explicit PlatformTray(QObject *parent = nullptr);
    ~PlatformTray() override;

    void setIcon(const QIcon &icon);
    void setToolTip(const QString &tip);
    void setContextMenu(QMenu *menu);
    void show();
    void hide();

    // Show a balloon / notification.
    void showMessage(const QString &title, const QString &message, const QIcon &icon = {}, int msecs = 5000);

    bool isAvailable() const;

signals:
    void activated();     // user clicked on the tray icon (single-click)
    void middleClicked(); // middle-click
    void doubleClicked(); // double-click (Windows)

private:
    void setupPlatformBackend();

#ifdef Q_OS_LINUX
    // Use D-Bus StatusNotifierItem if available (Wayland/KDE/GNOME).
    bool m_useSNI = false;
#endif

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
};
