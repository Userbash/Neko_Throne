// SPDX-License-Identifier: GPL-2.0-or-later
// SystemThemeWatcher — Detects OS dark/light theme and auto-switches.
// Windows: reads registry key; Linux: uses freedesktop portal.
// Emits themeChanged() so the UI layer can react instantly.

#pragma once

#include <QObject>

class QTimer;

class SystemThemeWatcher : public QObject {
    Q_OBJECT

public:
    enum class Theme { Light, Dark, Unknown };
    Q_ENUM(Theme)

    static SystemThemeWatcher *instance();

    Theme currentTheme() const;

    // Start watching for system theme changes.
    void start();
    void stop();

signals:
    void themeChanged(Theme newTheme);

private:
    explicit SystemThemeWatcher(QObject *parent = nullptr);
    ~SystemThemeWatcher() override = default;

    Theme detectSystemTheme() const;
    void pollTheme();

    Theme m_current = Theme::Unknown;
    QTimer *m_pollTimer = nullptr;

    Q_DISABLE_COPY_MOVE(SystemThemeWatcher)
};
