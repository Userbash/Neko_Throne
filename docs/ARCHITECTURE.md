# Neko Throne Architecture

This document describes the internal structure and design principles of the **Neko Throne** project.

## 🏗 High-Level Overview

Neko Throne follows a classic Client-Server (Frontend-Backend) architecture:

1.  **Frontend (GUI):** A C++ application built with **Qt 6.10.2**. It handles user interaction, profile management, routing configuration, and visualizes statistics.
2.  **Backend (Core):** A Go application (**NekoCore**) that embeds `sing-box` and `Xray-core`. It manages actual network connections, proxy protocols, and routing logic.
3.  **Communication:** The Frontend and Backend communicate via **gRPC**. The Frontend sends commands (start/stop proxy, update config) and receives real-time logs and traffic statistics.

## 📂 Core Components

### 🖥 Frontend (C++)
*   **`MainWindow`:** The central UI controller. Manages the system tray, profile list, and real-time status updates.
*   **`CoreManager`:** Orchestrates the lifecycle of the Go backend process.
*   **`DataStore`:** A centralized component for managing application state and persistent settings (JSON-based).
*   **`ProfileManager`:** Handles the storage, filtering, and sorting of proxy profiles.
*   **`CoreVersionParser`:** An asynchronous utility that queries core binaries for version information and availability.

### ⚙️ Backend (Go)
*   **`ThroneCore/server`:** The main entry point for the backend service.
*   **`internal/boxmain`:** Integration layer for `sing-box`.
*   **`internal/distro`:** Manages various core distributions and features.

## 🛠 Design Principles

### 1. Robustness and Safety
*   **Crash Handling:** Custom signal handlers capture SIGSEGV/SIGABRT and write detailed stack traces to `crash.log`.
*   **Environment Awareness:** The application detects restricted environments (like Distrobox/Headless) and automatically disables unstable GUI features (e.g., system tray updates) to prevent crashes.
*   **Strict Memory Management:** Uses smart pointers (`std::shared_ptr`, `std::unique_ptr`) and Qt's parent-child hierarchy to prevent memory leaks.

### 2. Performance
*   **Asynchronous Operations:** All blocking tasks (IO, process spawning, network requests) are performed on background threads using `QtConcurrent` or custom `QThread` pools.
*   **Optimized Compilation:** Uses `ccache` for faster incremental builds and specific compiler flags (`-O0 -g` for debug, `-O3` for release).

### 3. Portability
*   **Linux Integration:** Supports both Wayland and X11 via Qt abstraction. Handles SUID bits for TUN interface support.
*   **Windows Support:** Native Win32 API integration for service management and elevated permissions.

## 📡 Communication Protocol (gRPC)
The gRPC service (`libcore.proto`) defines the contract between the Frontend and Backend:
*   `StartCore`: Boots the proxy engine with a specific JSON configuration.
*   `StopCore`: Gracefully shuts down network interfaces.
*   `GetStats`: Streams real-time upload/download metrics.
*   `GetLogs`: Streams core execution logs for the internal log browser.
