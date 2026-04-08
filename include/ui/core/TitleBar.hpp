// SPDX-License-Identifier: GPL-2.0-or-later
// TitleBar — Custom title bar with Windows 11 Snap Layouts + Linux CSD support
//
// On Windows 11: frameless window with DWM Mica/Acrylic, native Snap Layouts via WM_NCHITTEST.
// On Windows 10: frameless window with flat translucent background (no Mica).
// On Linux/Wayland: respects compositor CSD; provides custom content area inside native frame.

#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>

class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parentWindow);
    ~TitleBar() override = default;

    void setTitle(const QString &title);
    void setIcon(const QIcon &icon);
    void setTitleBarVisible(bool visible);

    // Add a custom widget to the left side of the title bar (e.g. navigation buttons).
    void addLeftWidget(QWidget *w);
    // Add a custom widget to the center (e.g. search bar).
    void setCenterWidget(QWidget *w);

    // Apply platform-specific window flags and effects to parentWindow.
    // Call once after construction and show().
    static void applyPlatformWindowEffects(QWidget *window);

    QLabel *titleLabel() const { return m_titleLabel; }

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUI();
    void updateMaximizeButton();

    QWidget *m_parentWindow;
    QHBoxLayout *m_mainLayout = nullptr;
    QHBoxLayout *m_leftLayout = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QWidget *m_centerWidget = nullptr;
    QPushButton *m_btnMinimize = nullptr;
    QPushButton *m_btnMaximize = nullptr;
    QPushButton *m_btnClose = nullptr;
};

// ---------------------------------------------------------------------------
// Platform helpers (implemented per-platform in TitleBar_*.cpp)
// ---------------------------------------------------------------------------
namespace Platform {

// Enable Mica / Acrylic backdrop on Windows 11.
// No-op on other platforms.
void enableMicaEffect(QWidget *window);

// Returns true if the OS supports Mica (Windows 11 22000+).
bool isMicaSupported();

// Returns true if running under Wayland compositor.
bool isWayland();

// Configure CSD-compatible window hints for Linux/Wayland.
void configureLinuxCSD(QWidget *window);

// Install native event filter for Windows 11 Snap Layouts.
// Call after applyPlatformWindowEffects(). No-op on non-Windows.
void installSnapLayoutFilter(QWidget *window, TitleBar *titleBar);

}  // namespace Platform
