// SPDX-License-Identifier: GPL-2.0-or-later
// TitleBar platform helpers — Windows implementation
// Enables Mica/Acrylic on Win11, flat fallback on Win10.
// Provides native Snap Layout support via WM_NCHITTEST.

#ifdef _WIN32

#include "include/ui/core/TitleBar.hpp"
#include "include/sys/windows/WinVersion.h"

#include <QWidget>
#include <QWindow>
#include <QAbstractNativeEventFilter>
#include <QApplication>

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")

// ---- Undocumented DWM attributes for Mica / Acrylic backdrop ----
// These are stable since Windows 11 22H2 (build 22621).
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

// DWM_SYSTEMBACKDROP_TYPE numeric values — avoid redefinition on newer SDKs
// that already ship these as enum constants in dwmapi.h.
namespace {
    constexpr int kDwmSbtAuto            = 0;
    constexpr int kDwmSbtNone            = 1;
    constexpr int kDwmSbtMainWindow      = 2;   // Mica
    constexpr int kDwmSbtTransientWindow = 3;   // Acrylic
    constexpr int kDwmSbtTabbedWindow    = 4;   // Tabbed Mica
}

namespace Platform {

bool isMicaSupported() {
    // Windows 11 build 22000+
    return WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_11_21H2);
}

void enableMicaEffect(QWidget *window) {
    if (!window || !window->windowHandle())
        return;

    HWND hwnd = reinterpret_cast<HWND>(window->winId());

    // Extend frame into the entire client area so DWM backdrop shows through.
    MARGINS margins = {-1, -1, -1, -1};
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Enable dark mode for the title bar area if the system theme is dark.
    BOOL useDark = TRUE;
    ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

    // Set Mica backdrop (Windows 11 22H2+ with DWMWA_SYSTEMBACKDROP_TYPE).
    int backdropType = kDwmSbtMainWindow; // Mica
    HRESULT hr = ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));

    if (FAILED(hr)) {
        // Fallback for Windows 11 21H2 (build 22000-22620):
        // Use the older undocumented DWMWA_MICA_EFFECT (value 1029).
        static constexpr DWORD DWMWA_MICA_EFFECT = 1029;
        BOOL enableMica = TRUE;
        ::DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &enableMica, sizeof(enableMica));
    }
}

bool isWayland() {
    return false;
}

void configureLinuxCSD(QWidget *) {
    // No-op on Windows.
}

}  // namespace Platform

// ---------------------------------------------------------------------------
// Native event filter for Snap Layouts + window drag (Windows)
// ---------------------------------------------------------------------------

class Win32NativeEventFilter : public QAbstractNativeEventFilter {
public:
    explicit Win32NativeEventFilter(QWidget *window, TitleBar *titleBar)
        : m_window(window), m_titleBar(titleBar) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        if (eventType != "windows_generic_MSG")
            return false;

        auto *msg = static_cast<MSG *>(message);
        if (msg->hwnd != reinterpret_cast<HWND>(m_window->winId()))
            return false;

        switch (msg->message) {
        case WM_NCHITTEST: {
            // Let Windows know which parts of our custom title bar are draggable
            // and which button is the maximize button (for Snap Layouts).
            const int x = GET_X_LPARAM(msg->lParam);
            const int y = GET_Y_LPARAM(msg->lParam);

            RECT winRect;
            ::GetWindowRect(msg->hwnd, &winRect);

            // Resize borders (8px)
            constexpr int borderWidth = 8;
            if (y < winRect.top + borderWidth) { *result = HTTOP; return true; }
            if (y > winRect.bottom - borderWidth) { *result = HTBOTTOM; return true; }
            if (x < winRect.left + borderWidth) { *result = HTLEFT; return true; }
            if (x > winRect.right - borderWidth) { *result = HTRIGHT; return true; }

            // Map to widget coordinates
            QPoint localPos = m_titleBar->mapFromGlobal(QPoint(x, y));

            // If the cursor is within the title bar area
            if (m_titleBar->rect().contains(localPos)) {
                // Check if cursor is over the maximize button → HTMAXBUTTON for Snap Layouts
                if (m_titleBar->findChild<QPushButton *>(QStringLiteral("BtnMaximize"))
                        ->geometry().contains(localPos)) {
                    *result = HTMAXBUTTON;
                    return true;
                }
                // Check if cursor is over minimize / close → HTCLIENT (let Qt handle clicks)
                if (m_titleBar->findChild<QPushButton *>(QStringLiteral("BtnMinimize"))
                        ->geometry().contains(localPos) ||
                    m_titleBar->findChild<QPushButton *>(QStringLiteral("BtnClose"))
                        ->geometry().contains(localPos)) {
                    *result = HTCLIENT;
                    return true;
                }
                // Rest of title bar = draggable caption
                *result = HTCAPTION;
                return true;
            }
            break;
        }
        case WM_NCCALCSIZE: {
            // Remove the default non-client frame so our custom title bar fills the entire window.
            if (msg->wParam == TRUE) {
                *result = 0;
                return true;
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            // Prevent maximized window from covering the taskbar.
            auto *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            HMONITOR hMon = ::MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            ::GetMonitorInfo(hMon, &mi);
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            *result = 0;
            return true;
        }
        }
        return false;
    }

private:
    QWidget *m_window;
    TitleBar *m_titleBar;
};

// Factory function: installs the native filter on a window with a TitleBar.
// Call after TitleBar::applyPlatformWindowEffects().
namespace Platform {
void installSnapLayoutFilter(QWidget *window, TitleBar *titleBar) {
    auto *filter = new Win32NativeEventFilter(window, titleBar);
    QCoreApplication::instance()->installNativeEventFilter(filter);
}
}

#endif // _WIN32
