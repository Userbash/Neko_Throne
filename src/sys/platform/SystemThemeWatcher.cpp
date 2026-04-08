// SPDX-License-Identifier: GPL-2.0-or-later
#include "include/sys/platform/SystemThemeWatcher.hpp"

#include <QGuiApplication>
#include <QStyleHints>
#include <QTimer>

SystemThemeWatcher *SystemThemeWatcher::instance() {
    static SystemThemeWatcher s_instance;
    return &s_instance;
}

SystemThemeWatcher::SystemThemeWatcher(QObject *parent)
    : QObject(parent)
{
    m_current = detectSystemTheme();

    // Qt 6.5+ provides colorScheme() on QStyleHints.
    auto *hints = QGuiApplication::styleHints();
    connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
        Theme newTheme = (scheme == Qt::ColorScheme::Dark) ? Theme::Dark : Theme::Light;
        if (newTheme != m_current) {
            m_current = newTheme;
            emit themeChanged(m_current);
        }
    });
}

SystemThemeWatcher::Theme SystemThemeWatcher::currentTheme() const {
    return m_current;
}

void SystemThemeWatcher::start() {
    // The QStyleHints connection is already active.
    // For older Qt or platforms that don't emit the signal, use a fallback poll.
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(2000);
        connect(m_pollTimer, &QTimer::timeout, this, &SystemThemeWatcher::pollTheme);
    }
    m_pollTimer->start();
}

void SystemThemeWatcher::stop() {
    if (m_pollTimer)
        m_pollTimer->stop();
}

SystemThemeWatcher::Theme SystemThemeWatcher::detectSystemTheme() const {
    // Qt 6.5+ native detection
    auto scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
        return Theme::Dark;
    if (scheme == Qt::ColorScheme::Light)
        return Theme::Light;
    return Theme::Unknown;
}

void SystemThemeWatcher::pollTheme() {
    Theme detected = detectSystemTheme();
    if (detected != m_current && detected != Theme::Unknown) {
        m_current = detected;
        emit themeChanged(m_current);
    }
}
